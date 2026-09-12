# Runs inside the Windows guest, one stage per call, driven over the QEMU guest
# agent by tools/guest/run_guest_suite.py. Exit 3010 asks for a reboot before
# the next stage; any other non-zero exit fails the run.
#
#   prepare  once per image (run_guest_suite.py --prepare-image): the test
#            signing certificate and test signing, kernel crash dumps, verbose
#            device-install logging, and nothing that interrupts a run - fast
#            startup, sleep, automatic updates, maintenance, boot repair
#   sign     trust a self-signed test certificate, catalog and sign the package,
#            turn test signing on
#   install  install the package on virtio-gpu-rvk-pci, Driver Verifier's
#            standard checks on rvkgpu.sys
#   check    the adapter started on rvkgpu with no problem code; the verifier's
#            settings
#   collect  diagnostics into C:\rvk\diag, then one "file <path>" or
#            "dump <path>" line per file for the runner to pull
param(
    [Parameter(Mandatory = $true)][ValidateSet('prepare', 'sign', 'install', 'check', 'collect')][string]$Stage,
    [string]$Package = 'C:\rvk\package',
    [switch]$MemoryDump
)
$ErrorActionPreference = 'Stop'
$RebootRequired = 3010
$Subject = 'CN=accel-virt test signing'
$HardwareId = 'PCI\VEN_1AF4&DEV_1050&SUBSYS_52561AF4'
$Diagnostics = 'C:\rvk\diag'
$KernelMemoryDump = 2
$VerboseDeviceInstallLog = 0x2000FFFF

function Trust($Certificate, $StoreName) {
    $store = New-Object System.Security.Cryptography.X509Certificates.X509Store($StoreName, 'LocalMachine')
    $store.Open('ReadWrite')
    $store.Add($Certificate)
    $store.Close()
}

function Sign($Path, $Certificate) {
    $signature = Set-AuthenticodeSignature -FilePath $Path -Certificate $Certificate -HashAlgorithm SHA256
    if ($signature.Status -ne 'Valid') { throw "signing $Path left it $($signature.Status)" }
}

function SigningCertificate {
    $certificate = Get-ChildItem Cert:\LocalMachine\My | Where-Object Subject -eq $Subject | Select-Object -First 1
    if (-not $certificate) {
        $certificate = New-SelfSignedCertificate -Type CodeSigningCert -Subject $Subject `
            -CertStoreLocation Cert:\LocalMachine\My
    }
    Trust $certificate 'Root'
    Trust $certificate 'TrustedPublisher'
    $certificate
}

# Whether test signing had to be turned on, which takes a reboot.
function EnableTestSigning {
    if ((bcdedit /enum '{current}') -match 'testsigning\s+Yes') { return $false }
    bcdedit /set testsigning on | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "bcdedit /set testsigning on exited $LASTEXITCODE" }
    return $true
}

function SetDword($Path, $Name, $Value) {
    if (-not (Test-Path $Path)) { New-Item -Path $Path -Force | Out-Null }
    Set-ItemProperty -Path $Path -Name $Name -Value $Value -Type DWord
}

function RvkDevice {
    Get-PnpDevice -PresentOnly | Where-Object { $_.HardwareID -contains $HardwareId } | Select-Object -First 1
}

function SaveEvents($Log, $Count, $Name) {
    $path = "$Diagnostics\$Name"
    try {
        Get-WinEvent -LogName $Log -MaxEvents $Count -ErrorAction Stop |
            Format-List TimeCreated, Id, ProviderName, LevelDisplayName, Message |
            Out-File $path -Encoding utf8 -Width 400
    } catch {
        "no events from ${Log}: $($_.Exception.Message)" | Out-File $path -Encoding utf8
    }
}

switch ($Stage) {
    'prepare' {
        SigningCertificate | Out-Null
        # Fast startup resumes a hibernated kernel, which a copy-on-write
        # overlay booted on other hardware must not do.
        SetDword 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Power' HiberbootEnabled 0
        powercfg /hibernate off
        powercfg /change standby-timeout-ac 0
        powercfg /change monitor-timeout-ac 0
        $crash = 'HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl'
        SetDword $crash CrashDumpEnabled $KernelMemoryDump
        SetDword $crash AutoReboot 1
        SetDword $crash Overwrite 1
        SetDword $crash AlwaysKeepMemoryDump 1
        SetDword 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Setup' LogLevel $VerboseDeviceInstallLog
        SetDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate\AU' NoAutoUpdate 1
        SetDword 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Schedule\Maintenance' MaintenanceDisabled 1
        # A driver that bugchecks on every boot must keep rebooting into
        # Windows, where the runner notices, not stop in the recovery
        # environment, where nothing answers.
        bcdedit /set '{current}' recoveryenabled No | Out-Null
        bcdedit /set '{current}' bootstatuspolicy IgnoreAllFailures | Out-Null
        try {
            Add-MpPreference -ExclusionPath 'C:\rvk'
        } catch {
            Write-Output "prepare: no Defender exclusion: $($_.Exception.Message)"
        }
        Write-Output 'prepare: certificate, kernel dumps, install log, no fast startup, sleep, updates or repair'
        if (EnableTestSigning) { exit $RebootRequired }
        exit 0
    }
    'sign' {
        $certificate = SigningCertificate
        Sign "$Package\rvkgpu.sys" $certificate
        Remove-Item "$Package\rvkgpu.cat" -ErrorAction SilentlyContinue
        New-FileCatalog -Path $Package -CatalogFilePath "$Package\rvkgpu.cat" -CatalogVersion 2.0 | Out-Null
        Sign "$Package\rvkgpu.cat" $certificate
        if (EnableTestSigning) { exit $RebootRequired }
        exit 0
    }
    'install' {
        pnputil /add-driver "$Package\rvkgpu.inf" /install
        if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $RebootRequired) { exit $LASTEXITCODE }
        verifier /standard /driver rvkgpu.sys | Out-Null
        exit $RebootRequired
    }
    'check' {
        verifier /querysettings
        $device = RvkDevice
        if (-not $device) { Write-Output 'device: absent'; exit 1 }
        $property = { param($Key) (Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName $Key).Data }
        $problem = & $property DEVPKEY_Device_ProblemCode
        $status = '0x{0:X8}' -f [uint32](& $property DEVPKEY_Device_ProblemStatus)
        $service = & $property DEVPKEY_Device_Service
        # The desktop's compositor, and every driver bound to a display: feature
        # 038 asks for rvkgpu alone with dwm.exe composited on it.
        $dwm = @(Get-Process -Name dwm -ErrorAction SilentlyContinue).Count
        $displays = (Get-PnpDevice -Class Display -Status OK -ErrorAction SilentlyContinue |
            ForEach-Object { (Get-PnpDeviceProperty -InstanceId $_.InstanceId -KeyName DEVPKEY_Device_Service).Data }) -join ','
        Write-Output "display: dwm=$dwm services=$displays"
        # Last, so a failure's tail - all the runner reports - names the problem.
        Write-Output "device: $($device.InstanceId) status=$($device.Status) problem=$problem problem-status=$status service=$service"
        if ($device.Status -ne 'OK' -or $service -ne 'rvkgpu') { exit 1 }
        exit 0
    }
    'collect' {
        Remove-Item $Diagnostics -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $Diagnostics | Out-Null
        SaveEvents 'System' 400 'system.txt'
        SaveEvents 'Microsoft-Windows-DxgKrnl-Admin' 200 'dxgkrnl-admin.txt'
        SaveEvents 'Microsoft-Windows-DxgKrnl-Operational' 400 'dxgkrnl-operational.txt'
        SaveEvents 'Microsoft-Windows-Kernel-PnP/Configuration' 200 'kernel-pnp.txt'
        SaveEvents 'Microsoft-Windows-CodeIntegrity/Operational' 200 'code-integrity.txt'
        Copy-Item 'C:\Windows\INF\setupapi.dev.log' "$Diagnostics\setupapi.dev.log" -ErrorAction SilentlyContinue
        & {
            Get-PnpDevice -Class Display | Format-List FriendlyName, InstanceId, Status, Problem
            $device = RvkDevice
            if ($device) {
                Get-PnpDeviceProperty -InstanceId $device.InstanceId | Format-Table KeyName, Data -AutoSize -Wrap
            }
            sc.exe query rvkgpu
        } *>&1 | Out-File "$Diagnostics\device.txt" -Encoding utf8 -Width 400
        verifier /query *>&1 | Out-File "$Diagnostics\verifier.txt" -Encoding utf8 -Width 400
        Get-ChildItem $Diagnostics -File | ForEach-Object { "file $($_.FullName)" }
        $dumps = @(Get-ChildItem 'C:\Windows\Minidump', 'C:\Windows\LiveKernelReports' -Recurse -Filter *.dmp `
                -ErrorAction SilentlyContinue)
        if ($MemoryDump) { $dumps += @(Get-Item 'C:\Windows\MEMORY.DMP' -ErrorAction SilentlyContinue) }
        $dumps | ForEach-Object { "dump $($_.FullName)" }
        exit 0
    }
}
