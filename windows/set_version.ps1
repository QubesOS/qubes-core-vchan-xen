$version = Get-Content "../version"
# qubes version has 3 parts, windows needs 4
$version += ".0"
$version_str = "`"" + $version + "`""
$version = %{$version -replace "\.", ","}
$hdr = "#define QTW_FILEVERSION " + $version + "`n"
$hdr += "#define QTW_FILEVERSION_STR " + $version_str + "`n"
$hdr += "#define QTW_PRODUCTVERSION 3.0.0.0`n"
$hdr += "#define QTW_PRODUCTVERSION_STR `"3.0.0.0`"`n"
Set-Content -Path "version.h" $hdr
