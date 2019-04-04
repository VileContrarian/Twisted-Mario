// Fill out your copyright notice in the Description page of Project Settings.

#include "AVehicleBase.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Runtime/Engine/Classes/Engine/Engine.h" //For Printing Debug stuff

#define SMALLVAL 0.001f

// Sets default values
AVehicleBase::AVehicleBase()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

// Called when the game starts or when spawned
void AVehicleBase::BeginPlay()
{
	Super::BeginPlay();

	//Setting default attribute values
	if (mass <= 0.0f) mass = 100.0f;	//In Kg
	if (healthMax <= 0.0f) healthMax = 100;
	health = healthMax;

	if (attack <= 0) attack = 1;
	if (defense <= 0) defense = 1;

	if (force <= 0.0f) force = 10000.0f;
	if (torque <= 0.0f) torque = 250.0f;

	if (speedMax <= 0.0f) speedMax = 3000.0f;
	if (accelerationRate <= 0.0f) accelerationRate = 0.008f;
	if (accelerationDecay <= 0.0f) accelerationDecay = 50.0f;
	if (traction <= 0.0f) traction = 100.0f;
	if (handling <= 0.0f) handling = 50.0f;

	lapCounter = 0;
	timeElapsedLin = 1.0f;
	timeElapsedRot = 1.0f;
	accelDecayRange = 100.0f;
	tractionRange = 1000.0f;
	handlingRange = 100.0f;
	gripForce = 0.1f;
	driftGrip = 1.0f;
	climbSpeed = 0.5f;
	initialTraction = traction;
	isGrounded = true;
}

// Called every frame
void AVehicleBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	/*Debug Logs
	FRotator debugVel;
	debugVel = CarMesh->GetComponentRotation();
	UE_LOG(LogWindows, Warning, TEXT("Velocity : %f, %f, %f"), debugVel.Roll, debugVel.Pitch, debugVel.Yaw)
	//UE_LOG(LogWindows, Warning, TEXT("Velocity : %f, %f, %f"), sideForce.X, sideForce.Y, sideForce.Z)
	*/

	/*
	FString TheFloatStr = FString::SanitizeFloat(angleDeviation); //Convert float to string

	if (GEngine) {
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Blue, TEXT(""));
	}
	*/
}

void AVehicleBase::GetCarMesh(UStaticMeshComponent* CarMesh_)
{
	CarMesh = CarMesh_;
	CarMesh->SetupAttachment(Root);
}

//Move forward/backward
void AVehicleBase::Accelerate(float value_)
{
	if (CarMesh == nullptr) {
		return;
		//return FVector().ZeroVector;
	}

	//Checks button presses
	if (value_ > 0.0f) isForwardPressed = true;
	if (value_ < 0.0f) isForwardPressed = false;

	//If on the Air then just let residual velocity takes over
	if (!isGrounded) {
		//Don't do anything...maybe?
		return;
	}

	//Accel and Reverse
	if (value_ != 0.0f) {

		//Speed Limit condition
		if (GetSpeed() < speedMax) {
			timeElapsedAcc += accelerationRate;

			//float angularOrientation;

			velLinear.X = ((force / mass) * value_) * timeElapsedAcc * CarMesh->GetForwardVector().X;
			velLinear.Y = ((force / mass) * value_) * timeElapsedAcc * CarMesh->GetForwardVector().Y;
			velLinear.Z = ((force / mass) * value_) * timeElapsedAcc * CarMesh->GetForwardVector().Z * climbSpeed;

			//velLinear.Z = 0.0f;
		}

		else
		{
			//If moving beyond speed limit gain zero velocity
			velLinear = FVector().ZeroVector;
		}


		//Drifting counter force
		float angleDeviation;
		FVector counterForce;

		//Basically, if car's face is turning 90 degree away from current velocity then cancels out the velocity and recalculate
		if (isMovingForward()) {
			angleDeviation = FindAngle(CarMesh->GetComponentVelocity().GetSafeNormal(), CarMesh->GetForwardVector());

			counterForce = -CarMesh->GetComponentVelocity() * (FMath::Abs(angleDeviation) / 90.0f) / (tractionRange / traction);
			counterForce.Z = 0.0f;

			velLinear = velLinear + counterForce;
		}
		else {
			angleDeviation = FindAngle(CarMesh->GetComponentVelocity().GetSafeNormal(), -CarMesh->GetForwardVector());

			counterForce = -CarMesh->GetComponentVelocity() * (FMath::Abs(angleDeviation) / 90.0f) / (tractionRange / traction);
			counterForce.Z = 0.0f;

			velLinear = velLinear + counterForce;
		}

		CarMesh->SetPhysicsLinearVelocity(velLinear, true);

		timeElapsedLin = 1.0f;
	}

	//Accel decays when no input is received
	if (value_ == 0.0f) {
		if (accelerationDecay > accelDecayRange) accelerationDecay = accelDecayRange;
		if (accelerationDecay <= 0.0f) accelerationDecay = SMALLVAL;

		velLinear.X = -CarMesh->GetComponentVelocity().X / (accelDecayRange * 10.0f / accelerationDecay);
		velLinear.Y = -CarMesh->GetComponentVelocity().Y / (accelDecayRange * 10.0f / accelerationDecay);

		CarMesh->SetPhysicsLinearVelocity(velLinear, true);

		timeElapsedAcc = 0.0f;	//Reset acceleration rate
	}

	timeElapsedLin += FApp::GetFixedDeltaTime() / 1.0f;

	//return velLinear;
}

//Turn vehicle
void AVehicleBase::Turn(float value_)
{
	if (CarMesh == nullptr) {
		return;
		//return FVector().ZeroVector;
	}

	//Checks button presses
	turningDir = 1.0f / value_;

	//Calculate ratio to affect when and how fast the vehicle can turn
	float turningRate;

	if (GetSpeed() == 0.0f) {
		turningRate = 0.0f;
	}
	else {
		turningRate = GetSpeed() / speedMax;
	}

	//Rotation when moving forward
	if (value_ != 0.0f && isForwardPressed) {

		velAngular.X = 0.0f;
		velAngular.Y = 0.0f;
		velAngular.Z = (torque / mass) * turningDir * turningRate;

		CarMesh->SetPhysicsAngularVelocityInRadians(velAngular);
		timeElapsedRot = 2.0f;
	}

	//Rotation when reversing
	if (value_ != 0.0f && !isForwardPressed) {

		velAngular.X = 0.0f;
		velAngular.Y = 0.0f;
		velAngular.Z = -(torque / mass) * turningDir * (turningRate + 0.75f);


		CarMesh->SetPhysicsAngularVelocityInRadians(velAngular);
		timeElapsedRot = 2.0f;
	}

	//Residual angular drag when no longer receiving input
	if (value_ == 0.0f) {
		if (velAngular.Z != 0.0f) {
			velAngular.Z /= timeElapsedRot;
		}

		CarMesh->SetPhysicsAngularVelocityInRadians(velAngular, true);
	}

	if (handling > handlingRange) handling = handlingRange;
	if (handling <= 0.0f) handling = SMALLVAL;

	timeElapsedRot += FApp::GetFixedDeltaTime() / (handlingRange / handling);

	//return velAngular;
}

void AVehicleBase::HandBrake()
{
	traction = driftGrip;
}

void AVehicleBase::ReleaseHandBrake()
{
	traction = initialTraction;
}

void AVehicleBase::Jump()
{
	//Can jump only when on the ground
	if (isGrounded) {
		jumpForce.Z = 60000.0 / mass;

		isJumping = true;
		timeElapsedAir = 0.0f;

		CarMesh->SetPhysicsLinearVelocity(jumpForce, true);
		//return jumpForce;
	}
	//return FVector().ZeroVector;
}

void AVehicleBase::respawn(ACheckpoint *checkpoint)
{
	if (checkpoint->index == location) {
		CarMesh->SetWorldLocation(checkpoint->GetActorLocation());
		CarMesh->SetWorldRotation(checkpoint->GetActorRotation());
		velAngular.Set(0, 0, 0);
		velLinear.Set(0, 0, 0);
		accelAngular.Set(0, 0, 0);
		accelLinear.Set(0, 0, 0);
		timeElapsedAcc = 0;
		timeElapsedLin = 0;
		timeElapsedRot = 0;
		CarMesh->SetPhysicsLinearVelocity(FVector(0, 0, 0));
		CarMesh->SetPhysicsAngularVelocityInDegrees(FVector(0, 0, 0));
	}
}

void AVehicleBase::UseItem()
{
	//Will fill in later when item classes are set up
}

uint8 AVehicleBase::GetItemHeld()
{
	//Will fill in later...
	return 0;
}

void AVehicleBase::HandleEvents()
{
	//Will fill in later...
}

float AVehicleBase::GetSpeed()
{
	return FMath::Sqrt((CarMesh->GetComponentVelocity().X * CarMesh->GetComponentVelocity().X)
		+ (CarMesh->GetComponentVelocity().Y * CarMesh->GetComponentVelocity().Y));
}

void AVehicleBase::SetPosition(FVector vec_)
{
	if (CarMesh == nullptr) return;

	CarMesh->SetWorldLocation(vec_);
}

FVector AVehicleBase::GetPosition()
{
	if (CarMesh == nullptr) return FVector().ZeroVector;

	return CarMesh->GetComponentLocation();
}

void AVehicleBase::SetRotation(FVector vec_)
{
	if (CarMesh == nullptr) return;

	//CarMesh->SetPhysicsAngularVelocityInRadians(vec_);
	CarMesh->SetPhysicsAngularVelocityInDegrees(vec_);
}

FVector AVehicleBase::GetRotation()
{
	if (CarMesh == nullptr) return FVector().ZeroVector;

	//return CarMesh->GetPhysicsAngularVelocityInRadians();
	return CarMesh->GetPhysicsAngularVelocityInDegrees();
}

void AVehicleBase::DoDamage(int value_)
{
	if (value_ <= 0) return;
	if (health - value_ <= 0) health = 0;
	else health -= value_;
}

inline void AVehicleBase::DoHeal(int value_)
{
	if (value_ <= 0) return;
	if (health + value_ >= healthMax) health = healthMax;
	else health += value_;
}

int AVehicleBase::GetHealth()
{
	return health;
}

void AVehicleBase::SetMaxHealth(int value_)
{
	if (value_ > 0) healthMax = value_;
}

int AVehicleBase::GetMaxHealth()
{
	return healthMax;
}

void AVehicleBase::SetMass(float value_)
{
	if (value_ > 0.0f) mass = value_;
}

inline float AVehicleBase::GetMass()
{
	if (CarMesh == nullptr) return 0.0f;
	return CarMesh->GetMass();
}

void AVehicleBase::SetAttack(int value_)
{
	attack = value_;
}

int AVehicleBase::GetAttack()
{
	return attack;
}

void AVehicleBase::SetDefense(int value_)
{
	defense = value_;
}

int AVehicleBase::GetDefense()
{
	return defense;
}

void AVehicleBase::SetTraction(float value_)
{
	traction = value_;
}

inline float AVehicleBase::GetTraction()
{
	return traction;
}

inline void AVehicleBase::SetHandling(float value_)
{
	handling = value_;
}

inline float AVehicleBase::GetHandling()
{
	return handling;
}

inline void AVehicleBase::SetAccelerationRate(float value_)
{
	if (value_ > 0.0f) accelerationRate = value_;
}

inline float AVehicleBase::GetAccelerationRate()
{
	return accelerationRate;
}

inline void AVehicleBase::SetAccelerationDecay(float value_)
{
	if (value_ > 0.0f) accelerationDecay = value_;
}

inline float AVehicleBase::GetAccelerationDecay()
{
	return accelerationDecay;
}

bool AVehicleBase::isMovingForward()
{
	if (CarMesh == nullptr) return false;

	float directionMag;
	directionMag = FVector().DotProduct(CarMesh->GetForwardVector(), CarMesh->GetComponentVelocity());

	if (directionMag > 0.0f) return true; //Vehicle is moving forward

	return false;
}

FVector AVehicleBase::FindLeftVector()
{
	FVector forwardVel = CarMesh->GetComponentVelocity();
	FVector perpVector;

	perpVector.X = -forwardVel.Y;
	perpVector.Y = forwardVel.X;

	return perpVector;
}

FVector AVehicleBase::FindRightVector()
{
	FVector forwardVel = CarMesh->GetComponentVelocity();
	FVector perpVector;

	perpVector.X = forwardVel.Y;
	perpVector.Y = -forwardVel.X;

	return perpVector;
}

float AVehicleBase::FindAngle(FVector vecA, FVector vecB)
{
	float magA, magB;
	magA = FMath::Sqrt((vecA.X * vecA.X) + (vecA.Y * vecA.Y));
	magB = FMath::Sqrt((vecB.X * vecB.X) + (vecB.Y * vecB.Y));

	float angle = FMath::Acos(FVector().DotProduct(vecA, vecB) / (magA * magB));

	angle = FMath::RadiansToDegrees(angle);

	return angle;
}






