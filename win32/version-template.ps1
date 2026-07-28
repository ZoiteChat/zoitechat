param ([string] $templateFilename, [string] $outputFilename)

$versionParts = (Get-Content "${env:SOLUTIONDIR}VERSION" -Raw).Trim().Split('.')

[string[]] $contents = Get-Content $templateFilename -Encoding UTF8 | %{
	while ($_ -match '^(.*?)<#=(.*?)#>(.*?)$') {
		$_ = $Matches[1] + $(Invoke-Expression $Matches[2]) + $Matches[3]
	}
	$_
}

[System.IO.File]::WriteAllLines($outputFilename, $contents)
