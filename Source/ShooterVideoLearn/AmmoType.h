#pragma once
UENUM(BlueprintType)
enum class EAmmoType: uint8
{
	EAT_9MM UMETA(DisplayName = "9mm"),
	EAT_AR UMETA(DisplayName = "AR"),
	EAT_Sniper UMETA(DisplayName = "Sniper"),
	EAT_Max UMETA(DisplayName = "DefaultMAX"),
};