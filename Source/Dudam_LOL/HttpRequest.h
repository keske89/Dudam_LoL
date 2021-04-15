// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "HttpRequest.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRequestFinishedDelegate, bool, bIsSuccess, int, ResponseCode);

USTRUCT(Blueprintable)
struct DUDAM_LOL_API FUserData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly)
	int ChampionID;

	UPROPERTY(BlueprintReadOnly)
	int TeamNumber;

	UPROPERTY(BlueprintReadOnly)
	bool bIsWin;

	UPROPERTY(BlueprintReadOnly)
	int UserNumber;

	UPROPERTY(BlueprintReadOnly)
	FString UserName;
};


USTRUCT(Blueprintable)
struct DUDAM_LOL_API FGameData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly)
	int64 GameID;

	UPROPERTY(BlueprintReadOnly)
	int64 CreationTime;

	UPROPERTY(BlueprintReadOnly)
	int64 GameDuration;

	UPROPERTY(BlueprintReadOnly)
	int64 WinningTeamNumber;

	UPROPERTY(BlueprintReadOnly)
	TArray<FUserData> SummonerData;

	UPROPERTY(BlueprintReadOnly)
	bool bIsClanGame;

	UPROPERTY(BlueprintReadOnly)
	FString GameMode;

	UPROPERTY(BlueprintReadOnly)
	FString GameType;

	UPROPERTY(BlueprintReadOnly)
	int64 ClanMemberNum;

	UPROPERTY(BlueprintReadOnly)
	int64 QueueId;
};


/**
 * 
 */
UCLASS(BlueprintType)
class DUDAM_LOL_API UHttpRequest : public UObject
{
	GENERATED_BODY()
	
public:
	UHttpRequest(const class FObjectInitializer& ObjectInitializer);


	/////////////////////////////////// Request Riot API for Update GameData ///////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, DisplayName = "Requset Account Info by User Name", Category = "Dudam_Lol")
	void RequsetAccountbyUserName(FString UserName);
		
	void OnResponseReceviedByUserName(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	UFUNCTION(BlueprintCallable, DisplayName = "Requset MatchLists by Account ID", Category = "Dudam_Lol")
	void RequestMatchlistsByAccountId(FString EncryptedAccountID);

	void OnResponseReceviedMatchListByAccountID(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	UFUNCTION(BlueprintCallable, DisplayName = "Requset Game Data by Game ID", Category = "Dudam_Lol")
	void RequsetGameDataByGameID(int64 GameInstanceID);

	void OnResponseReceivedByGameID(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	
	UFUNCTION(BlueprintCallable, DisplayName = "Requset Clan Member List", Category = "Dudam_Lol")
	void RequestClanMemberList();

	void OnResponseReceivedClanMemberList(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	UFUNCTION(BlueprintCallable, DisplayName = "Save GameInstance_Json", Category = "Dudam_Lol")
	void SaveGameInstaceID(int64 CurrentGameID);

	/////////////////////////////////// Delegate ///////////////////////////////////////////////////////

	UPROPERTY(BlueprintAssignable)
	FRequestFinishedDelegate OnRequestFinishedCallback;


	//-----------------------------------------------------------------------------------------------//



	////////////////////////////////// Load From Local Json GameData for UI ////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, DisplayName = "Load UserInfo From Local DB", Category = "Dudam_Lol")
	bool LoadLocalUserInfo(FString SummonerID);


	UFUNCTION(BlueprintCallable, DisplayName = "Load GameInstance From Local DB", Category = "Dudam_Lol")
	bool LoadLocalGameData(int64 CurrentGameID);

	UFUNCTION(BlueprintCallable, DisplayName = "Load GameInstance From Local DB", Category = "Dudam_Lol")
	TMap<int64,FGameData> GetLoadedData();

	
	///////////////////////////////// RIOT API URL ///////////////////////////////////////////////
	UPROPERTY()
	FString SummonerV4_By_UserName = FString(TEXT("https://kr.api.riotgames.com/lol/summoner/v4/summoners/by-name/"));

	UPROPERTY()
	FString MatchV4_By_AccountID = FString(TEXT("https://kr.api.riotgames.com/lol/match/v4/matchlists/by-account/"));

	UPROPERTY()
	FString MatchV4_By_GameID = FString(TEXT("https://kr.api.riotgames.com/lol/match/v4/matches/"));


	///////////////////////////////// User Info /////////////////////////////////////////////////
	UPROPERTY(BlueprintReadOnly)
	FString SearchID;

	UPROPERTY(BlueprintReadOnly)
	FString PlayerAccountID;      //string	Encrypted account ID.Max length 56 characters.

	UPROPERTY(BlueprintReadOnly)
	int64 PlayerProfileIconId;	// int	ID of the summoner icon associated with the summoner.

	UPROPERTY(BlueprintReadOnly)
	int64 PlayerRevisionDate;	//long	Date summoner was last modified specified as epoch milliseconds.The following events will update this timestamp: summoner name change, summoner level change, or profile icon change.
	
	UPROPERTY(BlueprintReadOnly)
	FString	PlayerName;//	string	Summoner name.

	UPROPERTY(BlueprintReadOnly)
	FString PlayerId;	//string	Encrypted summoner ID.Max length 63 characters.

	UPROPERTY(BlueprintReadOnly)
	FString Playerpuuid;	//string	Encrypted PUUID.Exact length of 78 characters.

	UPROPERTY(BlueprintReadOnly)
	int64 PlayerSummonerLevel;	// long	Summoner level associated with the summoner.

	//---------------------------------------------------------------------------------------//

public:


	UPROPERTY(BlueprintReadOnly)
	TArray<int64> UserGameID;

	UPROPERTY(BlueprintReadOnly)
	TArray<int64> RequestGameID;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> SummonerID;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> ClanMemberList;

	UPROPERTY(BlueprintReadOnly)
	FGameData GameData;
	
	UPROPERTY(BlueprintReadOnly)
	TMap<int64, FGameData> LoadedData;
private:
	
	FHttpModule* HttpModule;
	
	UPROPERTY()
	FString MyAPI;
	
};