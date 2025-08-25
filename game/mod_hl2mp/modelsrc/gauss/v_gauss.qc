// Created by Crowbar 0.74

$modelname "Weapons/v_gauss.mdl"

$bodygroup "studio"
{
	studio "gauss_gun.smd"
}


$surfaceprop "default"

$contents "solid"

$illumposition -3.365 -27.951 -14.24

$cdmaterials "models\Weapons\V_hand\"
$cdmaterials "models\Weapons\v_gauss\"

$attachment "0" "Gauss Master" 12.5 -0.3 0 rotate 0 0 0
$attachment "muzzle" "muzzle" -0.00 -0.00 0.00 rotate -90.00 -90.00 0.00

$definebone "Hands biped" "" 0.000000 3.724260 39.395706 -0.000000 -89.999928 0.000000 0.000000 0.000000 0.000000 -0.000000 0.000000 0.000000
$definebone "Bip01 Pelvis" "Hands biped" 0.000000 0.000000 0.000000 -89.999983 -89.999928 0.000000 0.000000 0.000000 0.000000 -0.000000 0.000000 0.000000
$definebone "Bip01 Spine" "Bip01 Pelvis" 1.326317 -0.003921 0.000002 -0.002349 0.045034 -0.000229 0.000000 0.000000 0.000000 -0.000000 -0.000000 -0.000000
$definebone "Bip01 Spine1" "Bip01 Spine" 4.926304 -0.003875 -0.000197 0.000000 -0.000000 0.000000 0.000000 0.000000 0.000000 -0.000000 -0.000000 -0.000000
$definebone "Bip01 Spine2" "Bip01 Spine1" 4.926315 -0.003875 -0.000197 0.000000 -0.000000 0.000000 0.000000 0.000000 0.000000 -0.000000 -0.000000 -0.000000
$definebone "Bip01 Spine3" "Bip01 Spine2" 4.926315 -0.003875 -0.000197 0.000000 -0.000000 0.000000 0.000000 0.000000 0.000000 -0.000000 -0.000000 -0.000000
$definebone "Bip01 Neck" "Bip01 Spine3" 4.926304 -0.001461 -0.000197 0.000000 -0.000000 0.000000 0.000000 0.000000 0.000000 -0.000000 -0.000000 -0.000000
$definebone "Bip01 R Arm" "Bip01 Neck" -0.000076 0.001514 -1.894737 78.538591 179.954245 -0.000688 0.000000 0.000000 0.000000 -0.000001 -0.000000 -0.000000
$definebone "Bip01 R Arm1" "Bip01 R Arm" 4.926316 -0.000001 0.000004 -66.839590 -34.517321 35.226021 0.000000 -0.000001 0.000000 -0.000000 0.000000 -0.000000
$definebone "Bip01 R Arm2" "Bip01 R Arm1" 11.368427 0.000136 -0.000008 -0.000000 -95.965163 -0.000115 0.000000 0.000000 0.000000 0.000000 -0.000002 0.000000
$definebone "Bip01 R Hand" "Bip01 R Arm2" 11.368427 0.000008 -0.000007 7.019478 20.652322 92.592498 0.000000 0.000000 0.000004 -0.000000 0.000001 -0.000000
$definebone "Gauss Master" "Bip01 R Hand" 5.073439 0.334782 5.123142 -1.345247 9.938129 94.661867 0.000000 0.000000 0.000000 0.000002 0.000000 -0.000000
$definebone "muzzle" "Gauss Master" 11.804388 -0.000019 0.000020 0.000003 -0.000001 -0.000002 0.000000 0.000000 0.000000 0.000002 0.000000 -0.000000
$definebone "Spinner" "Gauss Master" -1.928781 -0.976109 -0.000206 0.000003 -0.000115 -0.000002 0.000000 0.000000 0.000000 0.000002 0.000000 -0.000000

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hboxset "default"
$hbox 0 "Gauss Master" -21.35 -4.75 -14.42 21.35 4.38 2.72
$hbox 0 "Spinner" -7.18 -2.45 -3.2 0.57 3.15 3.24

$sequence "idle2" {
	"v_gauss_anims\idle2.smd"
	activity "ACT_VM_IDLE" 1
	fadein 0.2
	fadeout 0.2
	fps 15
	loop
}

$sequence "fidget" {
	"v_gauss_anims\fidget.smd"
	activity "ACT_VM_FIDGET" 1
	fadein 0.2
	fadeout 0.2
	fps 30
}

$sequence "spinup" {
	"v_gauss_anims\spinup.smd"
	activity "ACT_VM_PULLBACK_LOW" 1
	fadein 0.2
	fadeout 0.2
	fps 30
}

$sequence "spin" {
	"v_gauss_anims\spin.smd"
	activity "ACT_VM_PULLBACK" 1
	fadein 0.2
	fadeout 0.2
	fps 30
	loop
}

$sequence "fire" {
	"v_gauss_anims\fire.smd"
	activity "ACT_VM_PRIMARYATTACK" 1
	fadein 0.2
	fadeout 0.2
	fps 30
}

$sequence "fire2" {
	"v_gauss_anims\fire2.smd"
	activity "ACT_VM_SECONDARYATTACK" 1
	fadein 0.2
	fadeout 0.2
	fps 30
}

$sequence "holster" {
	"v_gauss_anims\holster.smd"
	activity "ACT_VM_HOLSTER" 1
	fadein 0.2
	fadeout 0.2
	fps 45
}

$sequence "draw" {
	"v_gauss_anims\draw.smd"
	activity "ACT_VM_DRAW" 1
	fadein 0.2
	fadeout 0.2
	fps 60
}
