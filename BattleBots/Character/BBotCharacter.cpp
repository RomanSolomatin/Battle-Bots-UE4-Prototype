// Copyright 2015 VMR Games, Inc. All Rights Reserved.

#include "BattleBots.h"
#include "BBotCharacter.h"
#include "SpellSystem/SpellSystem.h"
#include "SpellSystem/DamageTypes/BBotDmgType_Holy.h"
#include "SpellSystem/DamageTypes/BBotDmgType_Fire.h"
#include "SpellSystem/DamageTypes/BBotDmgType_Ice.h"
#include "SpellSystem/DamageTypes/BBotDmgType_Lightning.h"
#include "SpellSystem/DamageTypes/BBotDmgType_Poison.h"
#include "SpellSystem/DamageTypes/BBotDmgType_Physical.h"


#define SPELL_BAR_SIZE 6

// Sets default values
ABBotCharacter::ABBotCharacter(const FObjectInitializer& ObjectInitializer)
  :Super(ObjectInitializer)
{
  // Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
  PrimaryActorTick.bCanEverTick = true;

  UCharacterMovementComponent* MoveComp = GetCharacterMovement();
  // Adjust jump to make it less floaty
  MoveComp->GravityScale = 1.5f;
  MoveComp->JumpZVelocity = 620;

  // Calls our custom collision handler
  GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ABBotCharacter::OnCollisionOverlap);

  //Initialize the size of the spell bar
  //spellBar_Internal.SetNum(SPELL_BAR_SIZE, false);

  // The combat stance index
  stanceIndex = 0;

  // Prevents multiple calls to takedamage by the same spell
  bTookDamage = false;
}

// Called when the game starts or when spawned
void ABBotCharacter::BeginPlay()
{
  Super::BeginPlay();

  GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Spawning arch char"));

  // Sets max health/oil to the default values
  maxHealth = health;
  maxOil = oil;

  APlayerController* pc = NULL;
  //playerController = Cast<ABattleBotsPlayerController>(this->GetController()->CastToPlayerController());
  for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator) {
    pc = Iterator->Get();

    if (pc->IsLocalPlayerController())
    {
      playerController = Cast<ABattleBotsPlayerController>(pc);
    }
  }
  //Init the 3 stances dependant on archetype - mage, warrior, etc
  InitCombatStances();
}

// Called every frame
void ABBotCharacter::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABBotCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
  Super::SetupPlayerInputComponent(InputComponent);
  InputComponent->BindAction("CastSpellOnRightClick", IE_Pressed, this, &ABBotCharacter::CastOnRightClick);
  InputComponent->BindAction("Jump", IE_Pressed, this, &ABBotCharacter::OnJumpStart);
  InputComponent->BindAction("Jump", IE_Released, this, &ABBotCharacter::OnJumpEnd);
  InputComponent->BindAction("ScrollUp", IE_Pressed, this, &ABBotCharacter::OnScrollUp);
  InputComponent->BindAction("ScrollDown", IE_Pressed, this, &ABBotCharacter::OnScrollDown);
}

// Is called when the player collides with something
void ABBotCharacter::OnCollisionOverlap(class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

}

float ABBotCharacter::GetCurrentHealth() const
{
  return health;
}

float ABBotCharacter::GetMaxHealth() const
{
  return maxHealth;
}

float ABBotCharacter::GetCurrentOil() const
{
  return oil;
}

// Pass in pos # to inc, or neg to decrement from current oil
void ABBotCharacter::SetCurrentOil(float decOil)
{
  if (Role < ROLE_Authority) {
    ServerSetCurrentOil(decOil);
  }
  else {
    oil = FMath::Clamp(oil + decOil, 0.f, GetMaxOil());
  }
}

void ABBotCharacter::ServerSetCurrentOil_Implementation(float decOil)
{
  SetCurrentOil(decOil);
}

bool ABBotCharacter::ServerSetCurrentOil_Validate(float decOil)
{
  return true;
}

float ABBotCharacter::GetMaxOil() const
{
  return maxOil;
}

bool ABBotCharacter::IsAlive() const
{
  return health > 0.f;
}

// Take damage and handle death
float ABBotCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
  // Prevents multiple TakeDamage calls by the same spell
  bTookDamage = true;

  if (health <= 0.f) {
    return 0.f;
  }

  const float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
  // Process damage post resist
  float DamageToApply = ProcessDamageTypes(ActualDamage, DamageEvent);

  if (DamageToApply > 0.f) {
    health -= DamageToApply;
  
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("I took damage") + FString::FromInt(DamageToApply));

    if (health <= 0) {
      Die();
    }
    else {
      // @todo: play hit animation
    }
  }

  return DamageToApply;
}

float ABBotCharacter::ProcessDamageTypes(float Damage, struct FDamageEvent const& DamageEvent)
{
  if (DamageEvent.DamageTypeClass == UBBotDmgType_Physical::StaticClass()) {
    return ProcessFinalDmgPostResist(Damage, characterConfig.physicalResist);
  }
  else if (DamageEvent.DamageTypeClass == UBBotDmgType_Ice::StaticClass()) {
    return ProcessFinalDmgPostResist(Damage, characterConfig.iceResist);
  }
  else if (DamageEvent.DamageTypeClass == UBBotDmgType_Lightning::StaticClass()) {
    return ProcessFinalDmgPostResist(Damage, characterConfig.lightningResist);
  }
  else if (DamageEvent.DamageTypeClass == UBBotDmgType_Holy::StaticClass()) {
    return ProcessFinalDmgPostResist(Damage, characterConfig.holyResist);
  }
  else if (DamageEvent.DamageTypeClass == UBBotDmgType_Poison::StaticClass()) {
    return ProcessFinalDmgPostResist(Damage, characterConfig.poisonResist);
  }
  else if (DamageEvent.DamageTypeClass == UBBotDmgType_Fire::StaticClass()) {
    return ProcessFinalDmgPostResist(Damage, characterConfig.fireResist);
  }
  else {
    return Damage;
  }
}


float ABBotCharacter::ProcessFinalDmgPostResist(float initialDmg, float currentResist)
{
  // If the resist is negative, then apply additional damage
  float resistModifier = 1 - FMath::Clamp(currentResist, -1.f, 1.f);
  return FMath::Abs(initialDmg * resistModifier);
}

bool ABBotCharacter::TookDamage() const
{
  return bTookDamage;
}

void ABBotCharacter::SetTookDamage(bool bDamaged)
{
  // Set the value locally
  bTookDamage = bDamaged;

  if (Role < ROLE_Authority) {
    ServerSetTookDamage(bDamaged);
  }
}

void ABBotCharacter::ServerSetTookDamage_Implementation(bool bDamaged)
{
  SetTookDamage(bDamaged);
}

bool ABBotCharacter::ServerSetTookDamage_Validate(bool bDamaged)
{
  return true;
}

void ABBotCharacter::Die()
{
  // @todo: implement death
  this->SetLifeSpan(0.1);
}

// Function called on right mouse click
void ABBotCharacter::CastOnRightClick()
{
  // Rotate to the direction of our mouse click
  RotateToMouseCursor();
  // Cast spell from our dedicated spell bar index
  CastFromSpellBar(0);
}

bool ABBotCharacter::CanCast(float spellCost) const
{
  return 0 <= (GetCurrentOil() - spellCost);
}

// Casts the spell at index
void ABBotCharacter::CastFromSpellBar(int32 index)
{
  if (Role < ROLE_Authority) {
    ServerCastFromSpellBar(index);
  }
  else {
    if (spellBar.IsValidIndex(index) && spellBar[index]->IsValidLowLevel()) {
      float currentTime = GetWorld()->GetTimeSeconds();

      if (GCDHelper < currentTime) {
        float spellCost = spellBar[index]->GetSpellCost();

        if (CanCast(spellCost)) {
          RotateToMouseCursor();
          spellBar[index]->CastSpell();
          SetCurrentOil(-spellCost);
          GCDHelper = currentTime + characterConfig.globalCooldown;
        }
      }
    }
  }
}

// Add a new spell to the spell bar
void ABBotCharacter::AddSpellToBar(TSubclassOf<ASpellSystem> newSpell)
{
  if (spellBar_Internal.Num() <= SPELL_BAR_SIZE) {
    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Adding spell"));
    spellBar_Internal.Add(newSpell);
    ASpellSystem* spell = GetWorld()->SpawnActor<ASpellSystem>(newSpell);
    spellBar.Add(spell);
  }
}

// Rotate player to mouse click location
void ABBotCharacter::RotateToMouseCursor()
{
  ABBotCharacter* const playerCharacter = this;
  if (playerCharacter) {
    // Get hit location under mouse click
    FHitResult hit;
    playerController->GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, hit);

    FVector mouseHitLoc = hit.Location;
    FVector characterLoc = playerCharacter->GetActorLocation();

    // Get the target location direction
    FVector targetLoc = (mouseHitLoc - characterLoc);
    targetLoc.Normalize();

    DrawDebugLine(GetWorld(), characterLoc, mouseHitLoc, FColor::Green, true, 0.5f, 0, 2.f);
    FRotator newRotation(0.f, targetLoc.Rotation().Yaw, 0.f);

    // Stop current movement,and face the new direction
    playerController->StopMovement();
    //playerCharacter->SetActorRotation(newRotation);
    playerCharacter->ChangeFacingRotation(newRotation);
  }
}


void ABBotCharacter::ChangeFacingRotation(FRotator newRotation)
{
  if (Role < ROLE_Authority) {
    ServerChangeFacingRotation(newRotation);
  }
//  else {
    this->SetActorRotation(newRotation);
//  }
}

void ABBotCharacter::ServerChangeFacingRotation_Implementation(FRotator newRotation)
{
  ChangeFacingRotation(newRotation);
}

bool ABBotCharacter::ServerChangeFacingRotation_Validate(FRotator newRotation)
{
  return true;
}

void ABBotCharacter::OnJumpStart()
{
  bPressedJump = true;
}

void ABBotCharacter::OnJumpEnd()
{
  bPressedJump = false;
}

bool ABBotCharacter::GetIsStunned() const
{
  return bIsStunned;
}

void ABBotCharacter::SetIsStunned(bool stunned)
{
  bIsStunned = stunned;

  if (Role < ROLE_Authority) {
    ServerSetIsStunned(stunned);
  }
}

void ABBotCharacter::ServerSetIsStunned_Implementation(bool stunned)
{
  SetIsStunned(stunned);
}

bool ABBotCharacter::ServerSetIsStunned_Validate(bool stunned)
{
  return true;
}

void ABBotCharacter::OnRep_IsStunned()
{
  this->DisableInput(playerController);
  // @todo: stop character movement while stunned
}

void ABBotCharacter::InitCombatStances()
{
  if (Role < ROLE_Authority) {
    ServerInitCombatStances();
  }

  // Must be overriden with Super::InitCombatStances()
}

void ABBotCharacter::ServerInitCombatStances_Implementation()
{
  InitCombatStances();
}

bool ABBotCharacter::ServerInitCombatStances_Validate()
{
  return true;
}

void ABBotCharacter::printCurrentStance()
{
  // Must override
}

void ABBotCharacter::OnRep_StanceChanged()
{
  // Must be overriden
}

// Getter for the current stance.
EStanceType ABBotCharacter::GetCurrentStance() const
{
  return currentStance;
}
// Setter for the current stance.
void ABBotCharacter::SetCurrentStance(EStanceType newStance)
{
  currentStance = newStance;

  if (Role < ROLE_Authority) {
    ServerSetCurrentStance(newStance);
  }
}

// Sets the current stance on the server
void ABBotCharacter::ServerSetCurrentStance_Implementation(EStanceType newStance)
{
  SetCurrentStance(newStance);
}

bool ABBotCharacter::ServerSetCurrentStance_Validate(EStanceType newStance)
{
  return true;
}

// Called on mouse wheel up
void ABBotCharacter::OnScrollUp()
{
  SwitchCombatStanceHelper(true);
  GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Scrolling UP"));
}

// Called on mouse wheel down
void ABBotCharacter::OnScrollDown()
{
  SwitchCombatStanceHelper(false);
  GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Scrolling Down"));
}

void ABBotCharacter::SwitchCombatStanceHelper(bool bScrolled)
{
  float currentTime = GetWorld()->GetTimeSeconds();
  // If the switchStance cd is up, call SwitchCombatStance
  if (switchStanceCDHelper < currentTime) {
    // Setting this to true increments the stance index
    bScrolledUp = bScrolled;
    SwitchCombatStance();
    // Reapply cooldown after switching stance
    switchStanceCDHelper = currentTime + switchStanceCoolDown;
  }
}

void ABBotCharacter::SwitchCombatStance()
{
  stanceIndex += bScrolledUp ? 1 : -1;
  int32 roundRobinIndex = FMath::Abs(stanceIndex) % combatStances.Num();

  if (combatStances.IsValidIndex(roundRobinIndex)) {
    currentStance = combatStances[roundRobinIndex];
  }
  printCurrentStance();
}

void ABBotCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);

  // Value is already updated locally, so we may skip it in replication step for the owner only
  DOREPLIFETIME_CONDITION(ABBotCharacter, currentStance, COND_SkipOwner);
  DOREPLIFETIME_CONDITION(ABBotCharacter, bIsStunned, COND_SkipOwner);
  DOREPLIFETIME_CONDITION(ABBotCharacter, bTookDamage, COND_SkipOwner);

  // Replicate to every client, no special condition required
  DOREPLIFETIME(ABBotCharacter, health);
  DOREPLIFETIME(ABBotCharacter, oil);
}

void ABBotCharacter::ServerCastFromSpellBar_Implementation(int32 index)
{
  CastFromSpellBar(index);
}

bool ABBotCharacter::ServerCastFromSpellBar_Validate(int32 index)
{
  return true;
}