# Makes the device ROM image of each of the HCP65's boards, ready for a programmer to burn into the board's SST39VF010
# (load the .bin at address 0; the rest of the chip is erased flash, $FF).  A board with a description written by hand
# in Boards\ is made from it; the others from their KiCad schematics, with the description worked out from the
# schematic written beside the image.
#
#	.\MakeImages.ps1 [-OutputDirectory <dir>]
#
# The images go in Images\ by default.  DeviceROM.exe must have been built (DeviceROM.sln, Release|x64).

param([string]$OutputDirectory = (Join-Path $PSScriptRoot 'Images'))

$DeviceROM = Join-Path $PSScriptRoot 'x64\Release\DeviceROM.exe'
$Boards = Join-Path $PSScriptRoot 'Boards'
$Kicad = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Kicad\Computer'))

# Each board's image and what it is made from: a description in Boards\, or a schematic in Kicad\Computer\.
$Images = @(
	@{ Image = 'SoundBoard';		Description = 'SoundBoard.xml' },
	@{ Image = 'SDCardBoard';		Description = 'SDCardBoard.xml' },
	@{ Image = 'UARTBoard';			Description = 'UARTBoard.xml' },
	@{ Image = 'VIABoard';			Description = 'VIABoard.xml' },
	@{ Image = 'MemoryBoard';		Description = 'MemoryBoard.xml' },
	@{ Image = 'BlitterBoard';		Schematic = 'Blitter\Blitter Board\Blitter Board.kicad_sch' },
	@{ Image = 'CoprocessorBoard';	Schematic = 'Coprocessor\Coprocessor Board - Main\Coprocessor Board - Main.kicad_sch' },
	@{ Image = 'GraphicsBoard';		Schematic = 'Graphics\Graphics Board\Graphics Board.kicad_sch' },
	@{ Image = 'LCDBoard';			Schematic = 'LCD\LCD Board\LCD Board.kicad_sch' },
	@{ Image = 'PS2Board';			Schematic = 'PS2\PS2 Board\PS2 Board.kicad_sch' },
	@{ Image = 'VideoBoard';		Schematic = 'Video\Video Board - Signals\Video Board - Signals.kicad_sch' }
)

if (-not (Test-Path $DeviceROM))
{
	Write-Host "Missing ${DeviceROM}: build DeviceROM.sln (Release|x64) first."
	exit 1
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null

$failed = @()
foreach ($entry in $Images)
{
	$bin = Join-Path $OutputDirectory ($entry.Image + '.bin')
	if ($entry.Description)
	{
		& $DeviceROM (Join-Path $Boards $entry.Description) $bin
	}
	else
	{
		$xml = Join-Path $OutputDirectory ($entry.Image + '.xml')
		& $DeviceROM (Join-Path $Kicad $entry.Schematic) $xml $bin
	}
	if ($LASTEXITCODE -ne 0)
	{
		$failed += $entry.Image
	}
}

if ($failed.Count -gt 0)
{
	Write-Host ("Failed: " + ($failed -join ', '))
	exit 1
}
Write-Host ("Made " + $Images.Count + " images in " + (Resolve-Path $OutputDirectory))
