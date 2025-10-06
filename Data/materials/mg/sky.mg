version "1"
MaterialType "Unlit"
$Color "COLOR"
textures
[
	{
		id "CUBEMAP_TEXTURE"
		TextureType "Cube"
		Filter "Linear"
		AddressU "Clamp"
		AddressV "Clamp"
		AddressW "Clamp"
	}
]
nodes
[
	{
		type "InPosition"
		id "POSITION"
	}
	{
		type "InViewPosition"
		id "VIEW_POSITION"
	}
	{
		type "Sub"
		id "DIRECTION"
		$A "POSITION"
		$B "VIEW_POSITION"
	}
	{
		type "TextureLoad"
		id "COLOR"
		$TexCoord "DIRECTION"
		$Texture "CUBEMAP_TEXTURE"
	}
]
