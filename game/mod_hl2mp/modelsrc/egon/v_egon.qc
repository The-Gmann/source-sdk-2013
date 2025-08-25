// Created by Crowbar 0.74

$modelname "weapons/v_egon.mdl"

$bodygroup "studio"
{
	studio "v_egon_reference.smd"
}


$surfaceprop "default"

$contents "solid"

$illumposition 20.728 -6.084 -7.257

$cdmaterials "models\Weapons\egon\"

$attachment "muzzle" "muzzle" 0 0 0 rotate -90 90 0

$cbox 0 0 0 0 0 0

$bbox 42.502 -36.168 -42.7 143.502 -15.375 -8.458

$hboxset "default"

$definebone "Main" "" -32.214993 -42.502361 -28.549683 -3.000007 -85.00001 0 0 0 0 0 0 0
$definebone "Others" "Main" 33.277557 -0.000003 5.723747 0 0 89.999982 0 0 0 0 0 0
$definebone "Middle" "Main" 59.199986 0 6.779303 0 0 89.999982 0 0 0 0 0 0
$definebone "Round" "Main" 62.799988 -0.000003 6.779303 0 0 89.999982 0 0 0 0 0 0
$definebone "Front" "Main" 86.299973 -0.000013 6.779304 0 0 89.999982 0 0 0 0 0 0
$definebone "muzzle" "Main" 101.09491 -0.000026 6.779285 0 0 89.999982 0 0 0 0 0 0


$sequence "idle" {
	"v_egon_anims\idle.smd"
	activity "ACT_VM_IDLE" 1
	fadein 0.2
	fadeout 0.2
	node "Ready"
	fps 30
	loop
}

$sequence "fire" {
	"v_egon_anims\fire.smd"
	activity "ACT_VM_PULLBACK" 1
	fadein 0.2
	fadeout 0.2
	node "Ready"
	fps 30
	loop
}

$sequence "holster" {
	"v_egon_anims\holster.smd"
	activity "ACT_VM_HOLSTER" 1
	fadein 0.2
	fadeout 0.2
	node "Ready"
	fps 30
}

$sequence "draw" {
	"v_egon_anims\draw.smd"
	activity "ACT_VM_DRAW" 1
	fadein 0.2
	fadeout 0.2
	node "Ready"
	fps 30
}

$sequence "idletolow" {
	"v_egon_anims\idletolow.smd"
	activity "ACT_VM_LOWERED_TO_IDLE" 1
	fadein 0.2
	fadeout 0.2
	transition "Ready" "Low"
	fps 30
}

$sequence "lowtoidle" {
	"v_egon_anims\lowtoidle.smd"
	activity "ACT_VM_IDLE_TO_LOWERED" 1
	fadein 0.2
	fadeout 0.2
	transition "Low" "Ready"
	fps 30
}

$sequence "lowidle" {
	"v_egon_anims\lowidle.smd"
	activity "ACT_VM_IDLE_LOWERED" 1
	fadein 0.2
	fadeout 0.2
	node "Ready"
	fps 30
	loop
}

$sequence "fidget" {
	"v_egon_anims\fidget.smd"
	activity "ACT_VM_FIDGET" 1
	fadein 0.2
	fadeout 0.2
	node "Ready"
	fps 30
}

$sequence "reload" {
	"v_egon_anims\reload.smd"
	activity "ACT_VM_RELOAD" 1
	fadein 0.2
	fadeout 0.2
	node "Ready"
	fps 30
}

$sequence "dryfire" {
	"v_egon_anims\dryfire.smd"
	activity "ACT_VM_DRYFIRE" 1
	fadein 0.2
	fadeout 0.2
	node "Fire"
	fps 30
}
