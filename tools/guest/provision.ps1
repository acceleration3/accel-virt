# Runs inside the Windows guest, one stage per call, driven over the QEMU guest
# agent by tools/guest/run_guest_suite.py. Exit 3010 asks for a reboot before
# the next stage; any other non-zero exit fails the run.
#
#   prepare  once per image (run_guest_suite.py --prepare-image): the test
#            signing certificate and test signing, kernel crash dumps, verbose
#            device-install logging, and nothing that interrupts a run - fast
#            startup, sleep, automatic updates, maintenance, boot repair
#   sign     name every present virtio-gpu-av-pci in the INF (below), trust a
#            self-signed test certificate, catalog and sign the package, turn
#            test signing on
#   install  install the package on virtio-gpu-av-pci, Driver Verifier's
#            standard checks on avgpu.sys - or, with -NoVerifier, without
#            them, for a run measuring what the verifier itself changes
#   check    every avgpu adapter started on avgpu with no problem code; the
#            verifier's settings
#   adapters what Windows says each display adapter is, one line per adapter
#            and source, '|'-separated: "wmi|<PNPDeviceID>|<AdapterRAM>|
#            <AdapterCompatibility>|<VideoProcessor>|<Name>" from
#            Win32_VideoController, and "dxgi|<VendorId>|<DeviceId>|<SubSysId>|
#            <Revision>|<DedicatedVideoMemory>|<SharedSystemMemory>|<Flags>|
#            <Description>" from IDXGIFactory1::EnumAdapters1, IDs in hex
#   modes    every display mode the driver offers, one "mode: WxH" line each,
#            and the one in force as "current: WxH"
#   setmode  switch the display to -Width by -Height; "set: <result>", where 0
#            is DISP_CHANGE_SUCCESSFUL and anything else is a refusal
#   extend   the desktop across every attached display; "extend: <result>",
#            where 0 is success
#   monitortimeout  the power policy's display timeout, in -Minutes; 0 never
#            turns it off. Needs no session: the system's idle timer is what
#            turns the display off, and that is the path a display driver must
#            survive
#   timeouts  the event 4101s in the System log, which is dxgkrnl saying it
#            reset a display driver that stopped responding, one "timeout:"
#            line each and a count
#   collect  diagnostics into C:\av\diag, then one "file <path>" or
#            "dump <path>" line per file for the runner to pull
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('prepare', 'sign', 'install', 'check', 'adapters', 'modes', 'setmode', 'extend', 'monitortimeout',
                 'sleepstates', 'hibernate', 'cycle', 'upgrade', 'timeouts', 'pageheap', 'heapchecks',
                 'collect')][string]$Stage,
    [string]$Package = 'C:\av\package',
    [int]$Width = 0,
    [int]$Height = 0,
    [int]$Minutes = 0,
    [switch]$MemoryDump,
    [switch]$NoVerifier,
    [switch]$Enable,
    [string[]]$Programs = @()
)
$ErrorActionPreference = 'Stop'
$RebootRequired = 3010
$Subject = 'CN=accel-virt test signing'
# virtio-gpu-av-pci presents its host GPU's vendor, device and revision
# (docs/protocol.md), which differ from host to host; its subsystem, 1af4:5256,
# is always this device's. virtio-gpu's own 1af4:1050 is what it keeps when its
# GPU has no PCI function, and the one ID the INF names statically.
$AvHardwareId = '^PCI\\VEN_([0-9A-F]{4})&DEV_([0-9A-F]{4})&SUBSYS_52561AF4$'
$StockHardwareId = 'PCI\VEN_1AF4&DEV_1050&SUBSYS_52561AF4'
# What the runner knows each host GPU is called: "<vendor>`t<device>`t<name>"
# a line, hex in lowercase. Absent, a generated model is named as the static one.
$AdapterNames = 'C:\av\adapters.txt'
# The end of every INF line WriteModels wrote, so signing again replaces them.
$GeneratedMark = '; provision.ps1: a host GPU'
$Diagnostics = 'C:\av\diag'
# The package's files and their SHA-256, as the runner built and pushed them
# (run_guest_suite.py's PACKAGE_MANIFEST), in sha256sum's form.
$Manifest = 'package.sha256'
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

# The device presents its host GPU's vendor and device (docs/protocol.md), so
# with a network Windows' device setup finds the vendor's own WHQL driver on
# Windows Update, ranks it above this test-signed one and installs it over
# the device - which then bugchecks at the next boot (2026-09-28: AMD's
# u0420529.inf on 1002:7550). NoAutoUpdate does not stop device setup, so it
# is told not to search Windows Update for drivers at all - and the policies
# do not stop an update already downloaded, which Windows Update installed
# after the next boot all the same, so its services and tasks are turned off
# and what it downloaded is dropped.
function NoDriverUpdates {
    SetDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate' ExcludeWUDriversInQualityUpdate 1
    SetDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\DriverSearching' DontSearchWindowsUpdate 1
    SetDword 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\DriverSearching' SearchOrderConfig 0
    SetDword 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Device Metadata' PreventDeviceMetadataFromNetwork 1
    foreach ($service in 'wuauserv', 'UsoSvc', 'WaaSMedicSvc') {
        Stop-Service $service -Force -ErrorAction SilentlyContinue
        SetDword "HKLM:\SYSTEM\CurrentControlSet\Services\$service" Start 4
    }
    Get-ScheduledTask -TaskPath '\Microsoft\Windows\WindowsUpdate\', '\Microsoft\Windows\UpdateOrchestrator\' `
        -ErrorAction SilentlyContinue | Disable-ScheduledTask -ErrorAction SilentlyContinue | Out-Null
    Remove-Item 'C:\Windows\SoftwareDistribution\Download\*' -Recurse -Force -ErrorAction SilentlyContinue
}

function AvDevices {
    @(Get-PnpDevice -PresentOnly | Where-Object { @($_.HardwareID | Where-Object { $_ -match $AvHardwareId }).Count })
}

function AvDevice {
    AvDevices | Select-Object -First 1
}

# The four-part hardware ID of every present virtio-gpu-av-pci, once each.
function AvHardwareIds {
    @(AvDevices | ForEach-Object { $_.HardwareID } | Where-Object { $_ -match $AvHardwareId } |
        ForEach-Object { $_.ToUpperInvariant() } | Sort-Object -Unique)
}

function AdapterName($Vendor, $Device) {
    if (-not (Test-Path $AdapterNames)) { return $null }
    foreach ($line in Get-Content $AdapterNames -Encoding UTF8) {
        $fields = $line -split "`t", 3
        if ($fields.Count -eq 3 -and $fields[0] -eq $Vendor -and $fields[1] -eq $Device) { return $fields[2] }
    }
    return $null
}

# One model line in the INF for each present virtio-gpu-av-pci whose IDs are
# its host GPU's: no static INF can list them, and PnP matches no ID made of a
# subsystem alone. Written before the catalog, which is what signs the INF.
# A host GPU the runner named gets its own DeviceDesc, which is what Device
# Manager, WMI and DXGI show until avgpu.sys writes the same name at
# StartDevice; the rest are named as the static model is.
function WriteModels($Inf) {
    $lines = [System.Collections.Generic.List[string]]@(Get-Content $Inf | Where-Object { -not $_.EndsWith($GeneratedMark) })
    $models = @()
    $strings = @()
    foreach ($id in AvHardwareIds) {
        if ($id -eq $StockHardwareId -or -not ($id -match $AvHardwareId)) { continue }
        $vendor = $Matches[1]
        $device = $Matches[2]
        $key = "HostGpu_${vendor}_${device}"
        $name = AdapterName $vendor.ToLowerInvariant() $device.ToLowerInvariant()
        $description = '%DeviceName%'
        if ($name) {
            $description = "%$key%"
            $strings += "$key = `"$($name.Replace('"', '""').Replace('%', '%%'))`" $GeneratedMark"
        }
        $models += "$description = Avgpu_Install, $id $GeneratedMark"
        Write-Output "sign: $id named $(if ($name) { $name } else { 'as the static model' })"
    }
    $section = $lines.FindIndex([Predicate[string]]{ param($line) $line -match '^\[Avgpu\.NTamd64\.10\.0\.\.\.26100\]' })
    if ($section -lt 0) { throw "$Inf has no [Avgpu.NTamd64.10.0...26100] section" }
    $lines.InsertRange($section + 1, [string[]]$models)
    $stringsAt = $lines.FindIndex([Predicate[string]]{ param($line) $line -match '^\[Strings\]' })
    if ($stringsAt -lt 0) { throw "$Inf has no [Strings] section" }
    $lines.InsertRange($stringsAt + 1, [string[]]$strings)
    # UTF-16 with a byte order mark: an INF may be ANSI or Unicode, and a
    # host GPU's name need not be ANSI.
    Set-Content -Path $Inf -Value $lines -Encoding Unicode
}

# Each file the manifest names, as the host built it. A guest that bugchecked
# mid-push kept avumd.dll's size and lost its bytes, and three upgrades
# installed it: every Direct3D 11 device failed and DWM restarted in a loop.
# Before sign changes avgpu.sys and the INF, every byte is still the host's.
# Both checks write with Write-Host: what a function outputs is what it returns.
function PackageMatchesManifest {
    $path = Join-Path $Package $Manifest
    if (-not (Test-Path $path)) { Write-Host 'package: no manifest, so no file is checked'; return $true }
    $bad = 0
    foreach ($line in Get-Content $path) {
        if ($line -notmatch '^([0-9a-fA-F]{64})\s+\*?(\S.*)$') { continue }
        $file = Join-Path $Package $Matches[2]
        $hash = if (Test-Path $file) { (Get-FileHash $file -Algorithm SHA256).Hash } else { 'missing' }
        if ($hash -ne $Matches[1].ToUpperInvariant()) {
            Write-Host "package: $($Matches[2]) is $hash, not the $($Matches[1]) the host built"
            $bad++
        }
    }
    if ($bad -eq 0) { Write-Host 'package: every file is the one the host built' }
    return $bad -eq 0
}

# The driver store's copy of each file the manifest names, against the
# package's as signing left it: the store's is what Windows loads, and pnputil
# copies without comparing. The store is the one the adapter's own driver key
# names its user-mode drivers from.
function StoreMatchesPackage {
    $path = Join-Path $Package $Manifest
    if (-not (Test-Path $path)) { return $true }
    $device = AvDevice
    if (-not $device) { Write-Host 'installed: the adapter is absent'; return $false }
    $key = (Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName DEVPKEY_Device_Driver).Data
    $names = @((Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\Class\$key").UserModeDriverName |
        Where-Object { $_ -and $_ -ne '<>' })
    if ($names.Count -eq 0) { Write-Host "installed: $key names no user-mode driver"; return $false }
    $store = Split-Path $names[0]
    $bad = 0
    foreach ($line in Get-Content $path) {
        if ($line -notmatch '^[0-9a-fA-F]{64}\s+\*?(\S.*)$') { continue }
        $name = $Matches[1]
        $installed = Join-Path $store $name
        $want = (Get-FileHash (Join-Path $Package $name) -Algorithm SHA256).Hash
        $got = if (Test-Path $installed) { (Get-FileHash $installed -Algorithm SHA256).Hash } else { 'missing' }
        if ($got -ne $want) { Write-Host "installed: $name in $store is $got, not the package's $want"; $bad++ }
    }
    if ($bad -eq 0) { Write-Host "installed: every packaged file in $store is the package's" }
    return $bad -eq 0
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

# EnumDisplaySettings and ChangeDisplaySettings: what the control panel uses,
# without a desktop session to open one in.
$DisplayApi = @'
using System;
using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public struct DEVMODE {
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmDeviceName;
    public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra;
    public int dmFields;
    public int dmPositionX, dmPositionY, dmDisplayOrientation, dmDisplayFixedOutput;
    public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmFormName;
    public short dmLogPixels;
    public int dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;
    public int dmICMMethod, dmICMIntent, dmMediaType, dmDitherType, dmReserved1, dmReserved2;
    public int dmPanningWidth, dmPanningHeight;
}

public class Display {
    [DllImport("user32.dll")]
    private static extern int SetDisplayConfig(uint numPathArrayElements, IntPtr pathArray,
                                               uint numModeInfoArrayElements, IntPtr modeInfoArray, uint flags);

    private const uint TopologyExtend = 0x00000004;
    private const uint Apply = 0x00000080;

    /* The desktop across every attached display, as DisplaySwitch /extend does.
       0 is success. */
    public static int Extend() {
        return SetDisplayConfig(0, IntPtr.Zero, 0, IntPtr.Zero, TopologyExtend | Apply);
    }

    [DllImport("user32.dll", CharSet = CharSet.Ansi)]
    private static extern bool EnumDisplaySettings(string device, int mode, ref DEVMODE devmode);
    [DllImport("user32.dll", CharSet = CharSet.Ansi)]
    private static extern int ChangeDisplaySettings(ref DEVMODE devmode, int flags);

    private const int CurrentSettings = -1;
    private const int FieldsSize = 0x00180000;   // DM_PELSWIDTH | DM_PELSHEIGHT
    private const int UpdateRegistry = 0x00000001;

    private static DEVMODE Empty() {
        DEVMODE devmode = new DEVMODE();
        devmode.dmSize = (short)Marshal.SizeOf(typeof(DEVMODE));
        return devmode;
    }

    /* "WxH" of the mode `index`, or "" past the last one. */
    public static string Mode(int index) {
        DEVMODE devmode = Empty();
        if (!EnumDisplaySettings(null, index, ref devmode)) return "";
        return devmode.dmPelsWidth + "x" + devmode.dmPelsHeight;
    }

    public static string Current() { return Mode(CurrentSettings); }

    /* What ChangeDisplaySettings answered: 0 is the switch. */
    public static int Switch(int width, int height) {
        DEVMODE devmode = Empty();
        if (!EnumDisplaySettings(null, CurrentSettings, ref devmode)) return int.MinValue;
        devmode.dmPelsWidth = width;
        devmode.dmPelsHeight = height;
        devmode.dmFields = FieldsSize;
        return ChangeDisplaySettings(ref devmode, UpdateRegistry);
    }
}
'@

# IDXGIFactory1::EnumAdapters1 and IDXGIAdapter1::GetDesc1, what an
# application listing adapters calls. Only the vtable slots before the two
# methods called matter; the placeholders are never called.
$DxgiApi = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
public struct DxgiAdapterDesc1 {
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string Description;
    public uint VendorId, DeviceId, SubSysId, Revision;
    public UIntPtr DedicatedVideoMemory, DedicatedSystemMemory, SharedSystemMemory;
    public uint LuidLowPart;
    public int LuidHighPart;
    public uint Flags;
}

[ComImport, Guid("29038f61-3839-4626-91fd-086879011a05"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IDxgiAdapter1 {
    void SetPrivateData(); void SetPrivateDataInterface(); void GetPrivateData(); void GetParent();
    void EnumOutputs(); void GetDesc(); void CheckInterfaceSupport();
    [PreserveSig] int GetDesc1(out DxgiAdapterDesc1 desc);
}

[ComImport, Guid("770aae78-f26f-4dba-a829-253c83d1b387"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IDxgiFactory1 {
    void SetPrivateData(); void SetPrivateDataInterface(); void GetPrivateData(); void GetParent();
    void EnumAdapters(); void MakeWindowAssociation(); void GetWindowAssociation(); void CreateSwapChain();
    void CreateSoftwareAdapter();
    [PreserveSig] int EnumAdapters1(uint index, out IDxgiAdapter1 adapter);
}

public static class Dxgi {
    [DllImport("dxgi.dll")]
    private static extern int CreateDXGIFactory1(ref Guid riid, [MarshalAs(UnmanagedType.Interface)] out IDxgiFactory1 factory);

    public static string[] Adapters() {
        List<string> lines = new List<string>();
        Guid iid = typeof(IDxgiFactory1).GUID;
        IDxgiFactory1 factory;
        int created = CreateDXGIFactory1(ref iid, out factory);
        if (created != 0) {
            lines.Add("dxgi-error|CreateDXGIFactory1 0x" + created.ToString("X8"));
            return lines.ToArray();
        }
        for (uint index = 0; ; index++) {
            IDxgiAdapter1 adapter;
            if (factory.EnumAdapters1(index, out adapter) != 0) break;
            DxgiAdapterDesc1 desc;
            int described = adapter.GetDesc1(out desc);
            if (described == 0) {
                lines.Add(string.Format("dxgi|{0:x4}|{1:x4}|{2:x8}|{3:x2}|{4}|{5}|{6}|{7}", desc.VendorId,
                                        desc.DeviceId, desc.SubSysId, desc.Revision, desc.DedicatedVideoMemory,
                                        desc.SharedSystemMemory, desc.Flags, desc.Description));
            } else {
                lines.Add("dxgi-error|GetDesc1 0x" + described.ToString("X8"));
            }
            Marshal.ReleaseComObject(adapter);
        }
        Marshal.ReleaseComObject(factory);
        return lines.ToArray();
    }
}
'@

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
        NoDriverUpdates
        SetDword 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Schedule\Maintenance' MaintenanceDisabled 1
        # A driver that bugchecks on every boot must keep rebooting into
        # Windows, where the runner notices, not stop in the recovery
        # environment, where nothing answers.
        bcdedit /set '{current}' recoveryenabled No | Out-Null
        bcdedit /set '{current}' bootstatuspolicy IgnoreAllFailures | Out-Null
        try {
            Add-MpPreference -ExclusionPath 'C:\av'
        } catch {
            Write-Output "prepare: no Defender exclusion: $($_.Exception.Message)"
        }
        Write-Output 'prepare: certificate, kernel dumps, install log, no fast startup, sleep, updates or repair'
        if (EnableTestSigning) { exit $RebootRequired }
        exit 0
    }
    'sign' {
        if (-not (PackageMatchesManifest)) { exit 1 }
        WriteModels "$Package\avgpu.inf"
        $certificate = SigningCertificate
        Sign "$Package\avgpu.sys" $certificate
        Remove-Item "$Package\avgpu.cat" -ErrorAction SilentlyContinue
        New-FileCatalog -Path $Package -CatalogFilePath "$Package\avgpu.cat" -CatalogVersion 2.0 | Out-Null
        Sign "$Package\avgpu.cat" $certificate
        if (EnableTestSigning) { exit $RebootRequired }
        exit 0
    }
    'install' {
        # Also here, for an image prepared before the prepare stage set it.
        NoDriverUpdates
        # The WDDM features this driver implements, staged off by this build of
        # Windows (feature 049, research.md §19). dxgkrnl's ConfigureFeature
        # reads Features\<DXGK_FEATURE_ID>\Enabled, and the driver's own INF
        # reaches only the adapter's database; the global one is here.
        #   4  DXGK_FEATURE_USER_MODE_SUBMISSION
        #   37 DXGK_FEATURE_NATIVE_FENCE
        foreach ($feature in 4, 37) {
            $key = "HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers\Features\$feature"
            SetDword $key Enabled 1
            SetDword $key AllowExperimental 1
        }
        pnputil /add-driver "$Package\avgpu.inf" /install
        if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $RebootRequired) { exit $LASTEXITCODE }
        if (-not (StoreMatchesPackage)) { exit 1 }
        if ($NoVerifier) {
            # Driver Verifier interposes on every DDI call, so a run that is
            # trying to find out whether the verifier is the thing refusing a
            # call has to be able to take it away (feature 053).
            Write-Output 'verifier: skipped, avgpu.sys installed with no Driver Verifier checks'
        } else {
            verifier /standard /driver avgpu.sys | Out-Null
            Write-Output 'verifier: standard checks on avgpu.sys'
        }
        exit $RebootRequired
    }
    'check' {
        verifier /querysettings
        $devices = @(AvDevices)
        if ($devices.Count -eq 0) { Write-Output 'device: absent'; exit 1 }
        # The desktop's compositor, and every driver bound to a display: feature
        # 038 asks for avgpu alone with dwm.exe composited on it.
        $dwm = @(Get-Process -Name dwm -ErrorAction SilentlyContinue).Count
        $displays = (Get-PnpDevice -Class Display -Status OK -ErrorAction SilentlyContinue |
            ForEach-Object { (Get-PnpDeviceProperty -InstanceId $_.InstanceId -KeyName DEVPKEY_Device_Service).Data }) -join ','
        # One monitor per output each adapter reports, which is what US4 asks.
        # Only the ones an avgpu adapter is the parent of: an image that has
        # run another display driver keeps its monitors.
        $instances = @($devices | ForEach-Object { $_.InstanceId })
        $monitors = @(Get-PnpDevice -Class Monitor -Status OK -ErrorAction SilentlyContinue | Where-Object {
            $instances -contains (Get-PnpDeviceProperty -InstanceId $_.InstanceId -KeyName DEVPKEY_Device_Parent).Data
        }).Count
        Write-Output "display: dwm=$dwm services=$displays monitors=$monitors adapters=$($devices.Count)"
        # Last, so a failure's tail - all the runner reports - names the problem.
        $failed = $false
        foreach ($device in $devices) {
            $property = { param($Key) (Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName $Key).Data }
            $problem = & $property DEVPKEY_Device_ProblemCode
            $status = '0x{0:X8}' -f [uint32](& $property DEVPKEY_Device_ProblemStatus)
            $service = & $property DEVPKEY_Device_Service
            Write-Output "device: $($device.InstanceId) status=$($device.Status) problem=$problem problem-status=$status service=$service"
            if ($device.Status -ne 'OK' -or $service -ne 'avgpu') { $failed = $true }
        }
        if ($failed) { exit 1 }
        exit 0
    }
    'adapters' {
        foreach ($controller in @(Get-CimInstance Win32_VideoController)) {
            Write-Output ("wmi|{0}|{1}|{2}|{3}|{4}" -f $controller.PNPDeviceID, $controller.AdapterRAM,
                          $controller.AdapterCompatibility, $controller.VideoProcessor, $controller.Name)
        }
        Add-Type -TypeDefinition $DxgiApi -ErrorAction Stop
        [Dxgi]::Adapters() | ForEach-Object { Write-Output $_ }
        exit 0
    }
    'modes' {
        Add-Type -TypeDefinition $DisplayApi -ErrorAction Stop
        $seen = @{}
        for ($index = 0; ; $index++) {
            $size = [Display]::Mode($index)
            if ($size -eq '') { break }
            if (-not $seen.ContainsKey($size)) { $seen[$size] = $true; Write-Output "mode: $size" }
        }
        $current = [Display]::Current()
        if ($current -eq '') { Write-Output 'current: none'; exit 1 }
        Write-Output "current: $current"
        exit 0
    }
    'setmode' {
        Add-Type -TypeDefinition $DisplayApi -ErrorAction Stop
        Write-Output "set: $([Display]::Switch($Width, $Height))"
        exit 0
    }
    'extend' {
        Add-Type -TypeDefinition $DisplayApi -ErrorAction Stop
        Write-Output "extend: $([Display]::Extend())"
        exit 0
    }
    'upgrade' {
        # A package with a higher DriverVer installed over the running one, as
        # an update would arrive: no reboot, and the adapter running on the new
        # version afterwards (feature 040, US2). The catalog is signed again
        # because the INF it covers changed.
        $inf = "$Package\avgpu.inf"
        $text = Get-Content $inf -Raw
        $match = [regex]::Match($text, 'DriverVer\s*=\s*([0-9/]+),([0-9.]+)')
        if (-not $match.Success) { Write-Output 'upgrade: no DriverVer in the package'; exit 1 }
        $parts = $match.Groups[2].Value.Split('.')
        $parts[2] = [string]([int]$parts[2] + $Minutes)
        $version = $parts -join '.'
        Set-Content $inf ($text -replace 'DriverVer\s*=\s*[0-9/]+,[0-9.]+', "DriverVer   = $($match.Groups[1].Value),$version")
        $certificate = SigningCertificate
        Remove-Item "$Package\avgpu.cat" -ErrorAction SilentlyContinue
        New-FileCatalog -Path $Package -CatalogFilePath "$Package\avgpu.cat" -CatalogVersion 2.0 | Out-Null
        Sign "$Package\avgpu.cat" $certificate
        pnputil /add-driver $inf /install
        if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $RebootRequired) { exit $LASTEXITCODE }
        $device = AvDevice
        if (-not $device) { Write-Output 'upgrade: the adapter is absent'; exit 1 }
        $running = (Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName DEVPKEY_Device_DriverVersion).Data
        $problem = (Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName DEVPKEY_Device_ProblemCode).Data
        Write-Output "upgrade: installed $version running $running problem $problem"
        if ($running -ne $version -or $problem -ne 0) { exit 1 }
        if (-not (StoreMatchesPackage)) { exit 1 }
        exit 0
    }

    'cycle' {
        # What an administrator does with pnputil, $Minutes times over: disable
        # the adapter and enable it again, checking it came back each time
        # (feature 040, US2). A cycle that leaves the device with a problem code
        # stops the loop where it happened.
        $device = AvDevice
        if (-not $device) { Write-Output 'cycle: the adapter is absent'; exit 1 }
        for ($cycle = 1; $cycle -le $Minutes; $cycle++) {
            Disable-PnpDevice -InstanceId $device.InstanceId -Confirm:$false -ErrorAction Stop
            Enable-PnpDevice -InstanceId $device.InstanceId -Confirm:$false -ErrorAction Stop
            $problem = (Get-PnpDeviceProperty -InstanceId $device.InstanceId `
                -KeyName DEVPKEY_Device_ProblemCode).Data
            if ($problem -ne 0) {
                Write-Output "cycle: $cycle of $Minutes left problem $problem"
                exit 1
            }
        }
        # What Driver Verifier tracked for this driver after the last cycle: a
        # pool allocation that outlives every disable is a leak.
        $pool = (verifier /query | Select-String -Pattern 'Current Pool Allocations|Pool Allocations') -join '; '
        Write-Output "cycle: $Minutes done"
        Write-Output "pool: $pool"
        exit 0
    }

    'sleepstates' {
        # What this machine actually offers. powercfg prints what is available
        # and then what is not, and the second list names the same states: only
        # the text before "not available" counts, or every state reads as
        # offered (feature 040, US1).
        $all = (powercfg /availablesleepstates | Out-String)
        $split = $all -split 'not available'
        $available = $split[0]
        $standby = if ($available -match 'Standby \(S3\)') { 'yes' } else { 'no' }
        $hibernate = if ($available -match 'Hibernate') { 'yes' } else { 'no' }
        Write-Output "sleep: standby=$standby hibernate=$hibernate"
        exit 0
    }

    'hibernate' {
        # The prepared image keeps hibernation off so nothing interrupts a run;
        # the S4 scenario turns it on inside the overlay, which is thrown away.
        powercfg /hibernate on
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        Write-Output 'hibernate: on'
        exit 0
    }

    'monitortimeout' {
        powercfg /change monitor-timeout-ac $Minutes
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        powercfg /change monitor-timeout-dc $Minutes
        Write-Output "timeout: $Minutes"
        exit $LASTEXITCODE
    }
    'timeouts' {
        # 4101: "Display driver avgpu stopped responding and has successfully
        # recovered" - what dxgkrnl logs once it has reset the engine
        # (feature 040, US3).
        $events = @(Get-WinEvent -FilterHashtable @{ LogName = 'System'; Id = 4101 } -ErrorAction SilentlyContinue)
        foreach ($event in $events) {
            $message = ($event.Message -replace '\s+', ' ')
            Write-Output "timeout: $($event.TimeCreated.ToString('s')) $message"
        }
        Write-Output "timeouts: $($events.Count)"
        exit 0
    }
    'pageheap' {
        # What gflags /p /enable writes, written directly: the debugging tools
        # are not in this image, and the flag is a registry value under Image
        # File Execution Options either way (feature 047, FR-006). 0x02000000
        # is FLG_HEAP_PAGE_ALLOCS; PageHeapFlags 3 is full page heap with
        # backwards-compatible padding, which is what catches a write past an
        # allocation at the moment it happens.
        foreach ($program in $Programs) {
            $key = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\$program"
            if ($Enable) {
                New-Item -Path $key -Force | Out-Null
                New-ItemProperty -Path $key -Name 'GlobalFlag' -Value '0x02000000' -PropertyType String -Force |
                        Out-Null
                New-ItemProperty -Path $key -Name 'PageHeapFlags' -Value '0x3' -PropertyType String -Force |
                        Out-Null
                Write-Output "pageheap: $program on"
            } else {
                Remove-Item -Path $key -Recurse -Force -ErrorAction SilentlyContinue
                Write-Output "pageheap: $program off"
            }
        }
        exit 0
    }
    'heapchecks' {
        # What page heap raises when it catches a write past an allocation: the
        # process is terminated and Windows Error Reporting records it, and a
        # verifier stop is logged under Application. Every event of either, so
        # a run reports what it found rather than only that it found nothing.
        $events = @(Get-WinEvent -FilterHashtable @{ LogName = 'Application' } -MaxEvents 200 `
                        -ErrorAction SilentlyContinue |
                Where-Object { $_.Message -match 'heap|verifier|0xc0000374|STATUS_HEAP_CORRUPTION' })
        foreach ($event in $events) {
            $message = ($event.Message -replace '\s+', ' ')
            Write-Output "heapcheck: $($event.Id) $message"
        }
        Write-Output "heapchecks: $($events.Count)"
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
        # The Vulkan ICD's log, one file per process that loaded it (feature
        # 045, FR-007); none when nothing did.
        Copy-Item 'C:\ProgramData\av\*.log' $Diagnostics -ErrorAction SilentlyContinue
        # What a traced program left when it crashed (docs/trace.md), and what
        # Windows Error Reporting recorded for any av program, traced or not:
        # a probe that faulted before its table existed shows only there.
        Copy-Item 'C:\ProgramData\av\crash-*.txt' $Diagnostics -ErrorAction SilentlyContinue
        Get-WinEvent -FilterHashtable @{ LogName = 'Application'; ProviderName = 'Windows Error Reporting' } `
                -MaxEvents 50 -ErrorAction SilentlyContinue |
            Where-Object { $_.Message -match 'av' } |
            Format-List TimeCreated, Message | Out-File "$Diagnostics\application-crashes.txt" -Encoding utf8
        & {
            Get-PnpDevice -Class Display | Format-List FriendlyName, InstanceId, Status, Problem
            $device = AvDevice
            if ($device) {
                Get-PnpDeviceProperty -InstanceId $device.InstanceId | Format-Table KeyName, Data -AutoSize -Wrap
            }
            sc.exe query avgpu
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
