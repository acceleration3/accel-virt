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
$AvHardwareId = '^PCI\\VEN_([0-9A-F]{4})&DEV_([0-9A-F]{4})&SUBSYS_52561AF4$'
$StockHardwareId = 'PCI\VEN_1AF4&DEV_1050&SUBSYS_52561AF4'
$AdapterNames = 'C:\av\adapters.txt'
$FallbackDriverVersions = @{
    '1002' = '32.0.31044.16'
    '10DE' = '32.0.16.1088'
    '8086' = '32.0.101.8509'
}
$GeneratedMark = '; provision.ps1: a host GPU'
$Diagnostics = 'C:\av\diag'
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
    $now = Get-Date
    $certificate = Get-ChildItem Cert:\LocalMachine\My |
        Where-Object { $_.Subject -eq $Subject -and $_.NotBefore -le $now -and $_.NotAfter -gt $now } |
        Select-Object -First 1
    if (-not $certificate) {
        $certificate = New-SelfSignedCertificate -Type CodeSigningCert -Subject $Subject `
            -CertStoreLocation Cert:\LocalMachine\My -NotBefore $now.AddDays(-2) -NotAfter $now.AddYears(10)
    }
    Trust $certificate 'Root'
    Trust $certificate 'TrustedPublisher'
    $certificate
}

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

function AvHardwareIds {
    @(AvDevices | ForEach-Object { $_.HardwareID } | Where-Object { $_ -match $AvHardwareId } |
        ForEach-Object { $_.ToUpperInvariant() } | Sort-Object -Unique)
}

function AdapterName($Vendor, $Device) {
    if (-not (Test-Path $AdapterNames)) { return $null }
    foreach ($line in Get-Content $AdapterNames -Encoding UTF8) {
        $fields = $line -split "`t", 4
        if ($fields.Count -ge 3 -and $fields[0] -eq $Vendor -and $fields[1] -eq $Device) { return $fields[2] }
    }
    return $null
}

function AdapterVersion($Vendor, $Device) {
    if (-not (Test-Path $AdapterNames)) { return $null }
    foreach ($line in Get-Content $AdapterNames -Encoding UTF8) {
        $fields = $line -split "`t", 4
        if ($fields.Count -eq 4 -and $fields[0] -eq $Vendor -and $fields[1] -eq $Device) {
            $version = $null
            if ([version]::TryParse($fields[3].Trim(), [ref]$version) -and $version.Revision -ge 0 -and
                @($version.Major, $version.Minor, $version.Build, $version.Revision | Where-Object { $_ -gt 65535 }).Count -eq 0) {
                return $version
            }
        }
    }
    return $null
}

function InfDriverVer($Inf) {
    foreach ($line in Get-Content $Inf) {
        $match = [regex]::Match($line, '^DriverVer\s*=\s*([0-9/]+),([0-9.]+)\s*$')
        if ($match.Success) { return [version]$match.Groups[2].Value }
    }
    throw "$Inf has no DriverVer"
}

function DriverVersions($Inf) {
    $asBuilt = InfDriverVer $Inf
    $given = @{}
    foreach ($id in AvHardwareIds) {
        if ($id -eq $StockHardwareId -or -not ($id -match $AvHardwareId)) { continue }
        $vendor = $Matches[1].ToLowerInvariant()
        $version = AdapterVersion $vendor $Matches[2].ToLowerInvariant()
        if (-not $given.ContainsKey($vendor) -or ($version -and (-not $given[$vendor] -or $version -gt $given[$vendor]))) {
            $given[$vendor] = $version
        }
    }
    if ($given.Count -eq 0) { $given['1af4'] = $null }
    $versions = [ordered]@{}
    foreach ($vendor in @($given.Keys | Sort-Object)) {
        if ($given[$vendor]) { $versions[$vendor] = $given[$vendor] }
        elseif ($FallbackDriverVersions.ContainsKey($vendor)) { $versions[$vendor] = [version]$FallbackDriverVersions[$vendor] }
        else { $versions[$vendor] = $asBuilt }
    }
    return $versions
}

$VersionResourceApi = @'
using System;
using System.Runtime.InteropServices;
public static class VersionResource {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr BeginUpdateResourceW(string file, bool deleteExisting);
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool UpdateResourceW(IntPtr update, IntPtr type, IntPtr name, ushort language, byte[] data, uint size);
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool EndUpdateResourceW(IntPtr update, bool discard);
}
'@

function VersionNode([string]$Key, [byte[]]$Value, [uint16]$ValueLength, [uint16]$Type, [byte[][]]$Children) {
    $bytes = New-Object 'System.Collections.Generic.List[byte]'
    $bytes.AddRange([byte[]](0, 0))
    $bytes.AddRange([BitConverter]::GetBytes($ValueLength))
    $bytes.AddRange([BitConverter]::GetBytes($Type))
    $bytes.AddRange([Text.Encoding]::Unicode.GetBytes($Key + [char]0))
    while ($bytes.Count % 4) { $bytes.Add(0) }
    if ($Value) {
        $bytes.AddRange($Value)
        while ($bytes.Count % 4) { $bytes.Add(0) }
    }
    foreach ($child in $Children) {
        $bytes.AddRange($child)
        while ($bytes.Count % 4) { $bytes.Add(0) }
    }
    $length = [BitConverter]::GetBytes([uint16]$bytes.Count)
    $bytes[0] = $length[0]
    $bytes[1] = $length[1]
    , $bytes.ToArray()
}

function VersionString([string]$Key, [string]$Value) {
    VersionNode $Key ([Text.Encoding]::Unicode.GetBytes($Value + [char]0)) ([uint16]($Value.Length + 1)) 1 @()
}

function StampVersion($Path, [version]$Version) {
    if (-not ('VersionResource' -as [type])) { Add-Type -TypeDefinition $VersionResourceApi -ErrorAction Stop }
    $text = $Version.ToString(4)
    $ms = ([uint32]$Version.Major -shl 16) -bor [uint32]$Version.Minor
    $ls = ([uint32]$Version.Build -shl 16) -bor [uint32]$Version.Revision
    $fixed = New-Object 'System.Collections.Generic.List[byte]'
    foreach ($field in [Convert]::ToUInt32('FEEF04BD', 16), [uint32]0x00010000, $ms, $ls, $ms, $ls, [uint32]0x3F,
                       [uint32]0, [uint32]0x00040004, [uint32]3, [uint32]4, [uint32]0, [uint32]0) {
        $fixed.AddRange([BitConverter]::GetBytes([uint32]$field))
    }
    $strings = @((VersionString 'CompanyName' 'accel-virt'),
                 (VersionString 'FileDescription' 'accel-virt Direct3D user-mode driver'),
                 (VersionString 'FileVersion' $text), (VersionString 'ProductName' 'accel-virt'),
                 (VersionString 'ProductVersion' $text))
    $table = VersionNode '040904B0' $null 0 1 $strings
    $stringInfo = VersionNode 'StringFileInfo' $null 0 1 @(, $table)
    $translation = VersionNode 'Translation' ([byte[]](0x09, 0x04, 0xB0, 0x04)) 4 0 @()
    $varInfo = VersionNode 'VarFileInfo' $null 0 1 @(, $translation)
    $blob = VersionNode 'VS_VERSION_INFO' $fixed.ToArray() 52 0 @($stringInfo, $varInfo)
    $update = [VersionResource]::BeginUpdateResourceW($Path, $false)
    if ($update -eq [IntPtr]::Zero) { throw "BeginUpdateResource $Path failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }
    if (-not [VersionResource]::UpdateResourceW($update, [IntPtr]16, [IntPtr]1, 1033, $blob, [uint32]$blob.Length)) {
        throw "UpdateResource $Path failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    if (-not [VersionResource]::EndUpdateResourceW($update, $false)) {
        throw "EndUpdateResource $Path failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
}

function StampUserModeDrivers($Versions) {
    Get-ChildItem $Package -Filter 'avumd*_*.dll' | Remove-Item -Force
    $targets = @()
    foreach ($vendor in $Versions.Keys) {
        foreach ($name in 'avumd', 'avumd32') {
            $target = "$name.dll"
            if ($targets.Count -ge 2) {
                $target = "${name}_$vendor.dll"
                Copy-Item "$Package\$name.dll" "$Package\$target"
            }
            $targets += , @($target, $Versions[$vendor])
        }
    }
    foreach ($target in $targets) {
        try {
            StampVersion "$Package\$($target[0])" $target[1]
            Write-Output "sign: $($target[0]) stamped $($target[1])"
        } catch {
            Write-Output "sign: $($target[0]) left unstamped: $($_.Exception.Message)"
        }
    }
}

function VendorInstallSection($Vendor) {
    $mark = $GeneratedMark
    $names = (1..4 | ForEach-Object { '"%13%\avumd_' + $Vendor + '.dll"' }) -join ', '
    $namesWow = (1..4 | ForEach-Object { '"%13%\avumd32_' + $Vendor + '.dll"' }) -join ', '
    @("[Avgpu_Install_$Vendor] $mark",
      "FeatureScore = F8 $mark",
      "CopyFiles    = Avgpu_Files, Avgpu_Vendor64, Avgpu_Vendor32, Avgpu_Files_$Vendor $mark",
      "AddReg       = Avgpu_SoftwareSettings, Avgpu_Scheduler, Avgpu_SoftwareSettings_$Vendor $mark",
      "[Avgpu_Install_$Vendor.HW] $mark",
      "AddReg = Avgpu_Interrupts $mark",
      "[Avgpu_Install_$Vendor.Services] $mark",
      "AddService = avgpu, 0x00000002, Avgpu_Service $mark",
      "[Avgpu_Files_$Vendor] $mark",
      "avumd_$Vendor.dll $mark",
      "avumd32_$Vendor.dll $mark",
      "[Avgpu_SoftwareSettings_$Vendor] $mark",
      "HKR,, UserModeDriverName, 0x00010000, $names $mark",
      "HKR,, UserModeDriverNameWow, 0x00010000, $namesWow $mark")
}

function WriteModels($Inf, $Versions) {
    $lines = [System.Collections.Generic.List[string]]@(Get-Content $Inf | Where-Object { -not $_.EndsWith($GeneratedMark) })
    $models = @()
    $strings = @()
    $later = @($Versions.Keys | Select-Object -Skip 1)
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
        $install = 'Avgpu_Install'
        if ($later -contains $vendor.ToLowerInvariant()) { $install = "Avgpu_Install_$($vendor.ToLowerInvariant())" }
        $models += "$description = $install, $id $GeneratedMark"
        Write-Output "sign: $id named $(if ($name) { $name } else { 'as the static model' })"
    }
    $section = $lines.FindIndex([Predicate[string]]{ param($line) $line -match '^\[Avgpu\.NTamd64\.10\.0\.\.\.26100\]' })
    if ($section -lt 0) { throw "$Inf has no [Avgpu.NTamd64.10.0...26100] section" }
    $lines.InsertRange($section + 1, [string[]]$models)
    $stringsAt = $lines.FindIndex([Predicate[string]]{ param($line) $line -match '^\[Strings\]' })
    if ($stringsAt -lt 0) { throw "$Inf has no [Strings] section" }
    $lines.InsertRange($stringsAt + 1, [string[]]$strings)
    $sections = @()
    $disks = @()
    foreach ($vendor in $later) {
        $sections += VendorInstallSection $vendor
        $disks += "avumd_$vendor.dll = 1 $GeneratedMark"
        $disks += "avumd32_$vendor.dll = 1 $GeneratedMark"
    }
    $lines.InsertRange($stringsAt, [string[]]$sections)
    $disksAt = $lines.FindIndex([Predicate[string]]{ param($line) $line -match '^\[SourceDisksFiles\]' })
    if ($disksAt -lt 0) { throw "$Inf has no [SourceDisksFiles] section" }
    $lines.InsertRange($disksAt + 1, [string[]]$disks)
    Set-Content -Path $Inf -Value $lines -Encoding Unicode
}

function RaiseDriverVer($Inf) {
    $device = AvDevice
    if (-not $device) { return }
    $provider = (Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName DEVPKEY_Device_DriverProvider).Data
    $running = (Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName DEVPKEY_Device_DriverVersion).Data
    if ($provider -ne 'accel-virt' -or -not $running) { return }
    $lines = @(Get-Content $Inf)
    for ($at = 0; $at -lt $lines.Count; $at++) {
        $match = [regex]::Match($lines[$at], '^DriverVer\s*=\s*([0-9/]+),([0-9.]+)\s*$')
        if (-not $match.Success) { continue }
        if ([version]$match.Groups[2].Value -gt [version]$running) { return }
        $parts = $running.Split('.')
        $parts[2] = [string]([int]$parts[2] + 1)
        $version = $parts -join '.'
        $lines[$at] = "DriverVer   = $($match.Groups[1].Value),$version"
        Set-Content -Path $Inf -Value $lines -Encoding Unicode
        Write-Output "sign: DriverVer $version, past the running $running"
        return
    }
}

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
    $files = @(foreach ($line in Get-Content $path) {
            if ($line -match '^[0-9a-fA-F]{64}\s+\*?(\S.*)$') { $Matches[1] }
        }) + @(Get-ChildItem $Package -Filter 'avumd*_*.dll' | ForEach-Object { $_.Name })
    foreach ($name in $files) {
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
        $versions = DriverVersions "$Package\avgpu.inf"
        StampUserModeDrivers $versions
        WriteModels "$Package\avgpu.inf" $versions
        RaiseDriverVer "$Package\avgpu.inf"
        $certificate = SigningCertificate
        Sign "$Package\avgpu.sys" $certificate
        Remove-Item "$Package\avgpu.cat" -ErrorAction SilentlyContinue
        New-FileCatalog -Path $Package -CatalogFilePath "$Package\avgpu.cat" -CatalogVersion 2.0 | Out-Null
        Sign "$Package\avgpu.cat" $certificate
        if (EnableTestSigning) { exit $RebootRequired }
        exit 0
    }
    'install' {
        NoDriverUpdates
        foreach ($feature in 4, 37) {
            $key = "HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers\Features\$feature"
            SetDword $key Enabled 1
            SetDword $key AllowExperimental 1
        }
        Remove-ItemProperty -Path 'HKLM:\SOFTWARE\Microsoft\Windows\Dwm' -Name DisableDDisplay -ErrorAction SilentlyContinue
        pnputil /add-driver "$Package\avgpu.inf" /install
        if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $RebootRequired) { exit $LASTEXITCODE }
        if (-not (StoreMatchesPackage)) { exit 1 }
        if ($NoVerifier) {
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
        $dwm = @(Get-Process -Name dwm -ErrorAction SilentlyContinue).Count
        $displays = (Get-PnpDevice -Class Display -Status OK -ErrorAction SilentlyContinue |
            ForEach-Object { (Get-PnpDeviceProperty -InstanceId $_.InstanceId -KeyName DEVPKEY_Device_Service).Data }) -join ','
        $instances = @($devices | ForEach-Object { $_.InstanceId })
        $monitors = @(Get-PnpDevice -Class Monitor -Status OK -ErrorAction SilentlyContinue | Where-Object {
            $instances -contains (Get-PnpDeviceProperty -InstanceId $_.InstanceId -KeyName DEVPKEY_Device_Parent).Data
        }).Count
        Write-Output "display: dwm=$dwm services=$displays monitors=$monitors adapters=$($devices.Count)"
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
        $pool = (verifier /query | Select-String -Pattern 'Current Pool Allocations|Pool Allocations') -join '; '
        Write-Output "cycle: $Minutes done"
        Write-Output "pool: $pool"
        exit 0
    }

    'sleepstates' {
        $all = (powercfg /availablesleepstates | Out-String)
        $split = $all -split 'not available'
        $available = $split[0]
        $standby = if ($available -match 'Standby \(S3\)') { 'yes' } else { 'no' }
        $hibernate = if ($available -match 'Hibernate') { 'yes' } else { 'no' }
        Write-Output "sleep: standby=$standby hibernate=$hibernate"
        exit 0
    }

    'hibernate' {
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
        $events = @(Get-WinEvent -FilterHashtable @{ LogName = 'System'; Id = 4101 } -ErrorAction SilentlyContinue)
        foreach ($event in $events) {
            $message = ($event.Message -replace '\s+', ' ')
            Write-Output "timeout: $($event.TimeCreated.ToString('s')) $message"
        }
        Write-Output "timeouts: $($events.Count)"
        exit 0
    }
    'pageheap' {
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
        Copy-Item 'C:\ProgramData\av\*.log' $Diagnostics -ErrorAction SilentlyContinue
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
