# Renders pages of a PDF to PNG files with the PDF engine built into Windows 10/11
# (Windows.Data.Pdf) - to look at generated documents without a PDF viewer, e.g. to check a
# layout change or to make pictures for docs/.
#
#   powershell -File tools/render_pdf.ps1 -Pdf out.pdf -OutDir pages [-Width 1600] [-Pages "0,3"]
#
# Pages are numbered from 0; output files are pageNN.png.

param([string]$Pdf, [string]$OutDir, [int]$Width = 1600, [string]$Pages = "")

Add-Type -AssemblyName System.Runtime.WindowsRuntime
$methods = [System.WindowsRuntimeSystemExtensions].GetMethods()
$asTaskOp = ($methods | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
$asTaskAction = ($methods | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncAction' })[0]

function Await($op, [Type]$type) {
    $task = $asTaskOp.MakeGenericMethod($type).Invoke($null, @($op))
    $task.Wait(-1) | Out-Null
    $task.Result
}
function AwaitAction($action) {
    $task = $asTaskAction.Invoke($null, @($action))
    $task.Wait(-1) | Out-Null
}

[Windows.Storage.StorageFile, Windows.Storage, ContentType = WindowsRuntime] | Out-Null
[Windows.Storage.StorageFolder, Windows.Storage, ContentType = WindowsRuntime] | Out-Null
[Windows.Data.Pdf.PdfDocument, Windows.Data.Pdf, ContentType = WindowsRuntime] | Out-Null
[Windows.Storage.Streams.IRandomAccessStream, Windows.Storage.Streams, ContentType = WindowsRuntime] | Out-Null

New-Item -ItemType Directory -Force $OutDir | Out-Null
$file = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync($Pdf)) ([Windows.Storage.StorageFile])
$doc = Await ([Windows.Data.Pdf.PdfDocument]::LoadFromFileAsync($file)) ([Windows.Data.Pdf.PdfDocument])
$folder = Await ([Windows.Storage.StorageFolder]::GetFolderFromPathAsync($OutDir)) ([Windows.Storage.StorageFolder])
Write-Output "pages: $($doc.PageCount)"
$list = if ($Pages) { $Pages.Split(',') | ForEach-Object { [int]$_ } } else { 0..($doc.PageCount - 1) }
foreach ($i in $list) {
    if ($i -ge $doc.PageCount) { continue }
    $page = $doc.GetPage([uint32]$i)
    $name = "page{0:D2}.png" -f $i
    $out = Await ($folder.CreateFileAsync($name, [Windows.Storage.CreationCollisionOption]::ReplaceExisting)) ([Windows.Storage.StorageFile])
    $stream = Await ($out.OpenAsync([Windows.Storage.FileAccessMode]::ReadWrite)) ([Windows.Storage.Streams.IRandomAccessStream])
    $options = New-Object Windows.Data.Pdf.PdfPageRenderOptions
    $options.DestinationWidth = [uint32]$Width
    AwaitAction ($page.RenderToStreamAsync($stream, $options))
    $stream.Dispose()
    $page.Dispose()
    Write-Output $name
}
