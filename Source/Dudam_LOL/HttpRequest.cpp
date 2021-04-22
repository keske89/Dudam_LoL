// Fill out your copyright notice in the Description page of Project Settings.


#include "HttpRequest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

//My API
//20 requests every 1 seconds(s)
//100 requests every 2 minutes(s)



UHttpRequest::UHttpRequest(const class FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	LoadedData.Empty();
	GameData.SummonerData.Empty();
	

	FFileHelper::LoadFileToString(MyAPI, *(FPaths::ProjectDir() + TEXT("API_key.txt"))); // read api key

	//RequsetClanMemberList();
	//FFileHelper::LoadFileToStringArray(ClanMemberList, *(FPaths::ProjectDir() + TEXT("ClanMemberList.txt"))); // Get Our Clan's MememberID
	//IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
}

void UHttpRequest::RequestClanMemberList()
{
	HttpModule = &FHttpModule::Get();
	TSharedPtr<IHttpRequest> HttpRequest = HttpModule->CreateRequest();
	
	
	FString URL = TEXT("https://drive.google.com/uc?export=download&id=11lRnbousKx8KYZnsYNiYBVAtLqCXewtz");

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(URL);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UHttpRequest::OnResponseReceivedClanMemberList);
	HttpRequest->ProcessRequest();
}

void UHttpRequest::OnResponseReceivedClanMemberList(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	FString BasePath = FPaths::ProjectDir();
	FString FileSavePath = BasePath + TEXT("ClanMemberList.txt");	

	// DownLoad Clan Member List File.
	if (Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode())) 
	{
		
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		IFileHandle* FileHandler = PlatformFile.OpenWrite(*FileSavePath);
		

		if (FileHandler)
		{

			FileHandler->Write(Response->GetContent().GetData(), Response->GetContentLength());
			FileHandler->Flush();
			delete FileHandler;
		}
		
	}
	else
	{
		UE_LOG(LogClass, Warning, TEXT("RequsetClanMemberList Error Code, %d"), (Response->GetResponseCode()));		
	}
	FFileHelper::LoadFileToStringArray(ClanMemberList, *(FileSavePath));

	
}



void UHttpRequest::RequsetAccountbyUserName(FString UserName)
{

	if (!LoadLocalUserInfo(UserName)) // Userinfo Not Exist // Request Riot API
	{
		HttpModule = &FHttpModule::Get();
		TSharedPtr<IHttpRequest> HttpRequest = HttpModule->CreateRequest();
		FString URL = SummonerV4_By_UserName + FGenericPlatformHttp::UrlEncode(UserName) + TEXT("?") + MyAPI;

		HttpRequest->SetVerb("GET");
		HttpRequest->SetURL(URL);
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UHttpRequest::OnResponseReceviedByUserName);
		HttpRequest->ProcessRequest();
	}
	else //Userinfo exist -> Request Match List by User Name
	{
		
		RequestMatchlistsByAccountId(PlayerAccountID);
	}


}



void UHttpRequest::OnResponseReceviedByUserName(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	///// Set User Data in Here.

	//Create a pointer to hold the json serialized data

	if (Response->GetResponseCode() == 200) // Requset Success
	{
		TSharedPtr<FJsonObject> JsonObject;

		//Create a reader pointer to read the json data
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

		//Deserialize the json data given Reader and the actual object to deserialize
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			PlayerName = JsonObject->GetStringField(TEXT("name"));
			PlayerAccountID = JsonObject->GetStringField(TEXT("AccountID"));
			PlayerProfileIconId = JsonObject->GetIntegerField(TEXT("ProfileIconId"));
			PlayerId = JsonObject->GetStringField(TEXT("Id"));
			Playerpuuid = JsonObject->GetStringField(TEXT("puuid"));
			PlayerSummonerLevel = JsonObject->GetIntegerField(TEXT("SummonerLevel"));
			PlayerRevisionDate = JsonObject->GetNumberField(TEXT("revisionDate"));


			// Write User data to Local Data Folder ,  Reducing Riot API Call.
			FString SaveFileName = PlayerName;
			FString Data;

			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Data);

			Writer->WriteObjectStart();
				
					Writer->WriteValue<FString>(TEXT("PlayerName"), PlayerName);
					Writer->WriteValue<FString>(TEXT("PlayerAccountID"), PlayerAccountID);
					Writer->WriteValue<int64>(TEXT("PlayerProfileIconId"), PlayerProfileIconId);
					Writer->WriteValue<FString>(TEXT("PlayerId"), PlayerId);
					Writer->WriteValue<FString>(TEXT("Playerpuuid"), Playerpuuid);
					Writer->WriteValue<int64>(TEXT("PlayerSummonerLevel"), PlayerSummonerLevel);
					Writer->WriteValue<int64>(TEXT("PlayerRevisionDate"), PlayerRevisionDate);
			
			Writer->WriteObjectEnd();
			Writer->Close();

			bool SaveSuccess = FFileHelper::SaveStringToFile(*Data, *(FPaths::ProjectDir() + TEXT("UserData/") + SaveFileName + TEXT(".Json")));
			if (SaveSuccess)
			{
				UE_LOG(LogClass, Warning, TEXT("SaveFile Success"));
			}
			else
			{
				UE_LOG(LogClass, Warning, TEXT("SaveFile Failed"));
			}

		}
	
		//RequestMatchlistsByAccountId(PlayerAccountID);
	}
	else 
	{
		UE_LOG(LogClass, Warning, TEXT("RequsetAccountbyUserName Error Code, %d"),(Response->GetResponseCode()));
	}

	OnSummonerRequestFinishedCallback.Broadcast(bWasSuccessful, Response->GetResponseCode());
}

void UHttpRequest::RequestMatchlistsByAccountId(FString EncryptedAccountID)
{
	// Get Lastest 100 Game's List
	HttpModule = &FHttpModule::Get();
	TSharedPtr<IHttpRequest> HttpRequest = HttpModule->CreateRequest();
	FString URL = MatchV4_By_AccountID + EncryptedAccountID + TEXT("?") + MyAPI;

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(URL);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UHttpRequest::OnResponseReceviedMatchListByAccountID);
	HttpRequest->ProcessRequest();	

}

void UHttpRequest::OnResponseReceviedMatchListByAccountID(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{

	if (Response->GetResponseCode() == 200)
	{
		UserGameID.Empty();
		LoadedData.Empty();

		TSharedPtr<FJsonObject> JsonObject;

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

		//Deserialize the json data given Reader and the actual object to deserialize
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			auto MatchObjectArr = JsonObject->GetArrayField("Matches");

			TSharedPtr<FJsonObject> MatchObejct;

			for (auto& arr : MatchObjectArr)
			{
				MatchObejct = arr->AsObject();

				UserGameID.Add(MatchObejct->GetNumberField("GameID"));
			}
		}

		for (int i = 0; i < 5;)
		{ // Get Lastest game data,
			if (UserGameID.IsValidIndex(0) && !LoadLocalGameData(UserGameID[0]))
			{//Data not Exist
				RequsetGameDataByGameID(UserGameID[0]);
				i++;
				UserGameID.RemoveAt(0);
			}				
			else if (UserGameID.IsValidIndex(0))
			{
				UserGameID.RemoveAt(0);
			}
			else
			{
				OnMatchInfoRequestFinishedCallback.Broadcast(bWasSuccessful, Response->GetResponseCode());
				break;
			}
			
		}
		OnMatchListRequestFinishedCallback.Broadcast(bWasSuccessful, Response->GetResponseCode());
	}	
	else
	{
		UE_LOG(LogClass, Warning, TEXT("RequestMatchlistsByAccountId Error Code, %d"), (Response->GetResponseCode()));
	}

	
}

void UHttpRequest::RequsetGameDataByGameID(int64 GameInstanceID)
{
	HttpModule = &FHttpModule::Get();
	TSharedPtr<IHttpRequest> HttpRequest = HttpModule->CreateRequest();
	FString URL = MatchV4_By_GameID + FString::Printf(TEXT("%lld"), GameInstanceID) + TEXT("?") + MyAPI;

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(URL);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UHttpRequest::OnResponseReceivedByGameID);
	HttpRequest->ProcessRequest();
}

void UHttpRequest::OnResponseReceivedByGameID(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (Response->GetResponseCode() == 200)
	{
		GameData.SummonerData.Empty();

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		
		//Deserialize the json data given Reader and the actual object to deserialize
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			GameData.ClanMemberNum = 0;
			GameData.bIsClanGame = false;
			GameData.QueueId = JsonObject->GetNumberField("QueueId");
			GameData.GameID = JsonObject->GetNumberField("gameId");
			GameData.CreationTime = (int64)JsonObject->GetNumberField("gameCreation") / (int64)1000; // ms to sec
			GameData.Date = (FDateTime::FromUnixTimestamp(GameData.CreationTime).ToString());
			GameData.GameDuration = JsonObject->GetNumberField("gameDuration");
			GameData.GameMode = JsonObject->GetStringField("gameMode");
			GameData.GameType = JsonObject->GetStringField("gameType");

			auto TeamIdArr = JsonObject->GetArrayField("teams");
			TSharedPtr<FJsonObject> TeamIDObject;

			for (auto& Team : TeamIdArr)
			{
				TeamIDObject = Team->AsObject();


				if (TEXT("win") == TeamIDObject->GetStringField("win"))
				{
					GameData.WinningTeamNumber = TeamIDObject->GetIntegerField("teamId");
				}
			}

			//get array about player info
			auto participantArr = JsonObject->GetArrayField("participants");
			TSharedPtr<FJsonObject> ParticipantObject;

			for (int i = 0; i < 10; i++)
			{
				ParticipantObject = participantArr[i]->AsObject();

				FUserData UserData;
				GameData.SummonerData.Add(UserData);
				

				GameData.SummonerData[i].UserNumber = ParticipantObject->GetIntegerField("participantId");
				GameData.SummonerData[i].TeamNumber = ParticipantObject->GetIntegerField("teamId");
				GameData.SummonerData[i].ChampionID = ParticipantObject->GetIntegerField("championId");

				if (GameData.WinningTeamNumber == GameData.SummonerData[i].TeamNumber)
				{
					GameData.SummonerData[i].bIsWin = true;
				}
				else
				{
					GameData.SummonerData[i].bIsWin = false;
				}
			}

			auto IdentitiesArr = JsonObject->GetArrayField("participantIdentities");
			TSharedPtr<FJsonObject> IdentitiesObject;

			for (auto& arr : IdentitiesArr)
			{
				IdentitiesObject = arr->AsObject();

				for (int i = 0; i < 10; i++)
				{
					if (GameData.SummonerData[i].UserNumber == IdentitiesObject->GetIntegerField("participantId"))
					{
						GameData.SummonerData[i].UserName = IdentitiesObject->GetObjectField("player")->GetStringField("summonerName");

						if (ClanMemberList.Contains(GameData.SummonerData[i].UserName))
						{
							GameData.SummonerData[i].bIsClanMember = true;
							GameData.ClanMemberNum++;
						}
						
					}
				}
				if (GameData.QueueId == 420 && GameData.ClanMemberNum > 1)// 클랜 듀오랭크
				{
					GameData.bIsClanGame = true;
				}
				else if (GameData.QueueId == 430 || GameData.QueueId == 440 || GameData.QueueId == 450 || GameData.QueueId == 900 || GameData.QueueId == 1020) //일반, 칼바람, 우르프, 단일챔피언 모드
				{
					if (GameData.ClanMemberNum > 2)
					{
						GameData.bIsClanGame = true;
					}
				}
			}

			SaveGameInstaceID(GameData.GameID);
			LoadedData.Add(GameData.GameID, GameData);

		}
		// ____________________________________________________________________________//
		else
		{
			UE_LOG(LogClass, Warning, TEXT("JsonObject is not valid"));
		}
		
	}
	else
	{
		UE_LOG(LogClass, Warning, TEXT("RequsetGameDataByGameID Error Code, %d"), (Response->GetResponseCode()));
	}

	OnMatchInfoRequestFinishedCallback.Broadcast(bWasSuccessful, Response->GetResponseCode());
}
	

void UHttpRequest::SaveGameInstaceID(int64 CurrentGameID)
{
	FString SaveFileName = FString::Printf(TEXT("%lld"), CurrentGameID);
	FString Data;

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Data);

	Writer->WriteObjectStart();
		Writer->WriteObjectStart("GameData");
			Writer->WriteValue<int64>(TEXT("CreationTime"), GameData.CreationTime);
			Writer->WriteValue<FString>(TEXT("Date"), GameData.Date);
			Writer->WriteValue<int64>(TEXT("GameDuration"), GameData.GameDuration);
			Writer->WriteValue<int64>(TEXT("WinningTeam"), GameData.WinningTeamNumber);
			Writer->WriteValue<int64>(TEXT("ClanMemberNum"), GameData.ClanMemberNum);
			Writer->WriteValue<bool>(TEXT("IsClanGame"), GameData.bIsClanGame);
			Writer->WriteValue<FString>(TEXT("GameMode"), GameData.GameMode);
			Writer->WriteValue<FString>(TEXT("GameType"), GameData.GameType);
			Writer->WriteValue<int64>(TEXT("QueueId"), GameData.QueueId);

		Writer->WriteObjectEnd();
		Writer->WriteObjectStart("SummonerData");

	for (int i = 0; i < GameData.SummonerData.Num(); i++)
	{
			Writer->WriteObjectStart(TEXT("Player") + FString::FromInt(i + 1));

				Writer->WriteValue<int>(TEXT("UserNumber"), GameData.SummonerData[i].UserNumber);
				Writer->WriteValue<int>(TEXT("ChampId"), GameData.SummonerData[i].ChampionID);
				Writer->WriteValue<int>(TEXT("TeamNumber"), GameData.SummonerData[i].TeamNumber);
				Writer->WriteValue<bool>(TEXT("isWin"), GameData.SummonerData[i].bIsWin);
				Writer->WriteValue<bool>(TEXT("isClanMember"), GameData.SummonerData[i].bIsClanMember);
				Writer->WriteValue<FString>(TEXT("UserName"), GameData.SummonerData[i].UserName);

			Writer->WriteObjectEnd();

	}
		Writer->WriteObjectEnd();
	Writer->WriteObjectEnd();
	
	Writer->Close();

	
	if (FFileHelper::SaveStringToFile(*Data, *(FPaths::ProjectDir() + TEXT("GameData/") + SaveFileName + TEXT(".Json"))))
	{
		UE_LOG(LogClass, Warning, TEXT("SaveFile Success"));
	}
	else
	{
		UE_LOG(LogClass, Warning, TEXT("SaveFile Failed"));
	}
}

FDateTime UHttpRequest::GetDateTime(int64 UnixTime)
{
	return FDateTime::FromUnixTimestamp(UnixTime);
}

bool UHttpRequest::LoadLocalGameData(int64 CurrentGameID)
{
	bool bLoadSuccess = false;

	// Already Loaded
	if (LoadedData.Find(CurrentGameID) != nullptr)
	{
		bLoadSuccess = true;
		return bLoadSuccess;
	}
	
	
	GameData.SummonerData.Empty();

	IFileManager* FileManager = &IFileManager::Get();
	TArray<FString> FoundFiles;
	FString TargetDir = FPaths::ProjectDir() + TEXT("GameData/");
	FString FileExtension = TEXT(".Json");
	FileManager->FindFiles(FoundFiles, *TargetDir, *FileExtension);
	FString CurrentFileName = FString::Printf(TEXT("%lld"), CurrentGameID) + TEXT(".Json");
	
	if (FoundFiles.FindByKey(CurrentFileName) != nullptr)
	{
		FString CurrentGameData;
		FFileHelper::LoadFileToString(CurrentGameData, *(TargetDir + CurrentFileName));

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CurrentGameData);
		TSharedPtr<FJsonObject> JsonObject;

		//Deserialize the json data given Reader and the actual object to deserialize
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{	// if Player is not Out clanMember . Do not Save user data

			GameData.GameID = CurrentGameID;
			GameData.CreationTime = JsonObject->GetObjectField(TEXT("GameData"))->GetNumberField(TEXT("CreationTime"));
			GameData.Date = JsonObject->GetObjectField(TEXT("GameData"))->GetStringField(TEXT("Date"));
			GameData.GameDuration = JsonObject->GetObjectField(TEXT("GameData"))->GetNumberField(TEXT("GameDuration"));
			GameData.WinningTeamNumber = JsonObject->GetObjectField(TEXT("GameData"))->GetNumberField(TEXT("WinningTeam"));
			GameData.ClanMemberNum = JsonObject->GetObjectField(TEXT("GameData"))->GetNumberField(TEXT("ClanMemberNum"));
			GameData.bIsClanGame = JsonObject->GetObjectField(TEXT("GameData"))->GetBoolField(TEXT("IsClanGame"));
			GameData.GameMode = JsonObject->GetObjectField(TEXT("GameData"))->GetStringField(TEXT("GameMode"));
			GameData.GameType = JsonObject->GetObjectField(TEXT("GameData"))->GetStringField(TEXT("GameType"));
			GameData.QueueId = JsonObject->GetObjectField(TEXT("GameData"))->GetNumberField(TEXT("QueueId"));

			for (int i = 0; i < 10; i++)
			{
				FUserData UserData;

				GameData.SummonerData.Add(UserData);

				GameData.SummonerData[i].UserNumber = JsonObject->GetObjectField(TEXT("SummonerData"))->GetObjectField(TEXT("Player" + FString::FromInt(i + 1)))->GetNumberField(TEXT("UserNumber"));
				GameData.SummonerData[i].ChampionID = JsonObject->GetObjectField(TEXT("SummonerData"))->GetObjectField(TEXT("Player" +  FString::FromInt(i + 1)))->GetNumberField(TEXT("ChampId"));
				GameData.SummonerData[i].TeamNumber = JsonObject->GetObjectField(TEXT("SummonerData"))->GetObjectField(TEXT("Player" +  FString::FromInt(i + 1)))->GetNumberField(TEXT("TeamNumber"));
				GameData.SummonerData[i].bIsWin = JsonObject->GetObjectField(TEXT("SummonerData"))->GetObjectField(TEXT("Player" +  FString::FromInt(i + 1)))->GetBoolField(TEXT("isWin"));
				GameData.SummonerData[i].bIsClanMember = JsonObject->GetObjectField(TEXT("SummonerData"))->GetObjectField(TEXT("Player" + FString::FromInt(i + 1)))->GetBoolField(TEXT("isClanMember"));
				GameData.SummonerData[i].UserName = JsonObject->GetObjectField(TEXT("SummonerData"))->GetObjectField(TEXT("Player" +  FString::FromInt(i + 1)))->GetStringField(TEXT("UserName"));
			}
			LoadedData.Add(GameData.GameID,GameData);
			bLoadSuccess = true;		
		}	
	}
	return bLoadSuccess;
}


bool UHttpRequest::LoadLocalUserInfo(FString UserSummonerID)
{
	bool bLoadSuccess = false;

	if (ClanMemberList.Find(UserSummonerID))
	{
		FString UserData;
		FString FilePath = FPaths::ProjectDir() + TEXT("UserData/") + UserSummonerID + TEXT(".Json");
		FFileHelper::LoadFileToString(UserData, *FilePath);

		//Create a reader pointer to read the json data
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(UserData);

		TSharedPtr<FJsonObject> JsonObject;
		//Deserialize the json data given Reader and the actual object to deserialize
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{	// if Player is not Out clanMember . Do not Save user data

			PlayerName = JsonObject->GetStringField(TEXT("PlayerName"));
			PlayerAccountID = JsonObject->GetStringField(TEXT("PlayerAccountID"));
			PlayerProfileIconId = JsonObject->GetIntegerField(TEXT("PlayerProfileIconId"));
			PlayerId = JsonObject->GetStringField(TEXT("PlayerId"));
			Playerpuuid = JsonObject->GetStringField(TEXT("Playerpuuid"));
			PlayerSummonerLevel = JsonObject->GetIntegerField(TEXT("PlayerSummonerLevel"));
			PlayerRevisionDate = JsonObject->GetIntegerField(TEXT("PlayerRevisionDate"));
			
			bLoadSuccess = true;
		}
		else
		{
			bLoadSuccess = false;
		}
	}
	return bLoadSuccess;
}


TMap<int64, FGameData> UHttpRequest::GetLoadedData()
{
	return LoadedData;
}