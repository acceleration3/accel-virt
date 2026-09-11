# Runs inside the Windows guest, one stage per call, driven over the QEMU guest
# agent by tools/guest/run_guest_suite.py. Exit 3010 asks for a reboot before
# the next stage; any other non-zero exit fails the run.
#
#   sign     trust a self-signed test certificate, catalog and sign the package,
#            turn test signing on
#   install  install the package on virtio-gpu-rvk-pci, Driver Verifier's
#            standard checks on rvkgpu.sys
#   check    the adapter started on rvkgpu with no problem code; the verifier's
#            settings
param(
    [Parameter(Mandatory = $true)][ValidateSet('sign', 'install', 'check')][string]$Stage,
    [string]$Package = 'C:\rvk\package'
)
$ErrorActionPreference = 'Stop'
$RebootRequired = 3010
$Subject = 'CN=accel-virt test signing'
$HardwareId = 'PCI\VEN_1AF4&DEV_1050&SUBSYS_52561AF4'

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

switch ($Stage) {
    'sign' {
        $certificate = Get-ChildItem Cert:\LocalMachine\My | Where-Object Subject -eq $Subject | Select-Object -First 1
        if (-not $certificate) {
            $certificate = New-SelfSignedCertificate -Type CodeSigningCert -Subject $Subject `
                -CertStoreLocation Cert:\LocalMachine\My
        }
        Trust $certificate 'Root'
        Trust $certificate 'TrustedPublisher'
        Sign "$Package\rvkgpu.sys" $certificate
        Remove-Item "$Package\rvkgpu.cat" -ErrorAction SilentlyContinue
        New-FileCatalog -Path $Package -CatalogFilePath "$Package\rvkgpu.cat" -CatalogVersion 2.0 | Out-Null
        Sign "$Package\rvkgpu.cat" $certificate
        if ((bcdedit /enum '{current}') -match 'testsigning\s+Yes') { exit 0 }
        bcdedit /set testsigning on | Out-Null
        exit $RebootRequired
    }
    'install' {
        pnputil /add-driver "$Package\rvkgpu.inf" /install
        if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $RebootRequired) { exit $LASTEXITCODE }
        verifier /standard /driver rvkgpu.sys | Out-Null
        exit $RebootRequired
    }
    'check' {
        $device = Get-PnpDevice -PresentOnly | Where-Object { $_.HardwareID -contains $HardwareId } |
            Select-Object -First 1
        if (-not $device) { Write-Output 'device: absent'; exit 1 }
        $problem = (Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName DEVPKEY_Device_ProblemCode).Data
        $service = (Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName DEVPKEY_Device_Service).Data
        Write-Output "device: $($device.InstanceId) status=$($device.Status) problem=$problem service=$service"
        verifier /querysettings
        if ($device.Status -ne 'OK' -or $service -ne 'rvkgpu') { exit 1 }
        exit 0
    }
}
