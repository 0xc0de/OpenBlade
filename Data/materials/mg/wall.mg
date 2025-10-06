version "1"
MaterialType "PBR"
AllowScreenSpaceReflections "false"
$Color "DIFFUSE"
$Metallic "=0"
$Roughness "=1"
textures
[
	{
		id "TEXTURE_DIFFUSE"
		Filter "Trilinear"
	}
]
nodes
[
	{
		id "TEXCOORD"
		type "InTexCoord"
	}
	{
		id "DIFFUSE"
		type "TextureLoad"
		$TexCoord "TEXCOORD"
		$Texture "TEXTURE_DIFFUSE"
	}
]
