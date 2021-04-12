// Fill out your copyright notice in the Description page of Project Settings.


#include "HttpRequest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Kismet/GameplayStatics.h"

//My API
//20 requests every 1 seconds(s)
//100 requests every 2 minutes(s)



UHttpRequest::UHttpRequest(const class FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	HttpModule = &FHttpModule::Get();
	BlockTimer = 120.f;
	BlockRequest = false;
	CurrentIndex = 0;
	LoadedGameData.Empty();

	FFileHelper::LoadFileToString(MyAPI, *(FPaths::ProjectDir() + TEXT("API_key.txt"))); // read api key

	RequsetClanMemberList();
	//FFileHelper::LoadFileToStringArray(ClanMemberList, *(FPaths::ProjectDir() + TEXT("ClanMemberList.txt"))); // Get Our Clan's MememberID
	//IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
}

void UHttpRequest::RequsetClanMemberList()
{
	TSharedPtr<IHttpRequest> HttpRequest = HttpModule->CreateRequest();
	
	
	FString URL = TEXT("https://drive.google.com/uc?export=download&id=11lRnbousKx8KYZnsYNiYBVAtLqCXewtz");

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(URL);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UHttpRequest::OnResponseReceivedClanMemberList);
	HttpRequest->ProcessRequest();
}

void UHttpRequest::OnResponseReceivedClanMemberList(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{


	FString BasePath = FPaths::FPaths::ProjectDir();
	FString FileSavePath = BasePath + TEXT("ClanMemberList.txt");	

	// DownLoad Clan Member List File.
	if (Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode())) 
	{
		
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		IFileHandle* FileHandler = PlatformFile.OpenWrite(*FileSavePath);
		FFileHelper::LoadFileToStringArray(ClanMemberList, *(FileSavePath));

		if (FileHandler)
		{
	
			FileHandler->Write(Response->GetContent().GetData(), Response->GetContentLength());
			FileHandler->Flush();		

			delete FileHandler;
			bWasSuccessful = true;
		}
	}
	else
	{
		FFileHelper::LoadFileToStringArray(ClanMemberList, *(FileSavePath));
	}
}



void UHttpRequest::RequsetAccountbyUserName(FString UserName)
{
	if (!LoadLocalUserInfo(UserName)) // Userinfo Not Exist // Request Riot API
	{
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
		else if(Response->GetResponseCode() == 429) 
		{
			UE_LOG(LogClass, Warning, TEXT("Rate limit exceeded"));
		}

		RequestMatchlistsByAccountId(PlayerAccountID);
	}
	else
	{
		//UE_LOG(LogClass, Warning, (Response->GetResponseCode()));
	}
	
}

void UHttpRequest::RequestMatchlistsByAccountId(FString EncryptedAccountID)
{
	// Get Lastest 100 Game's List

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
		else
		{
			UE_LOG(LogClass, Warning, TEXT("JsonObject is not valid"));
		}

	

		for (int i = 0; i < 5;)
		{
			if (!LoadLocalGameData(UserGameID[i]))
			{
				RequsetGameDataByGameID(UserGameID[i]);
				UserGameID.Remove(0);
				i++;
			}		
		}
	}
	else
	{
		UE_LOG(LogClass, Warning, TEXT("RequestMatchlistsByAccountId Failed"));
	}
	
	
}

void UHttpRequest::RequsetGameDataByGameID(int64 GameInstanceID)
{
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
		for (int i = 0; i < 10; i++)
		{
			FUserData UserData;
			GameData.SummonerData.Add(UserData);
		}



		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

		//Deserialize the json data given Reader and the actual object to deserialize
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			GameData.ClanMemberNum = 0;
			GameData.bIsClanGame = false;
			GameData.QueueId = JsonObject->GetNumberField("QueueId");
			GameData.GameID = JsonObject->GetNumberField("gameId");
			GameData.CreationTime = JsonObject->GetNumberField("gameCreation");
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
					}

				}
			}


			for (int i = 0; i < GameData.SummonerData.Num(); i++)
			{
				for (int k = 0; k < ClanMemberList.Num(); k++)
				{
					if (GameData.SummonerData[i].UserName == ClanMemberList[k])
					{
						GameData.ClanMemberNum++;

					}
				}
				if (GameData.QueueId == 420 && GameData.ClanMemberNum > 1)// 클랜 듀오랭크
				{
					GameData.bIsClanGame = true;
				}
				else if (GameData.QueueId == 430 || GameData.QueueId == 440 || GameData.QueueId == 450 || GameData.QueueId == 900) //일반, 칼바람, 우르프
				{
					if (GameData.ClanMemberNum > 2)
					{
						GameData.bIsClanGame = true;
					}
				}
			}

		}
		else
		{
			UE_LOG(LogClass, Warning, TEXT("JsonObject is not valid"));
		}

		SaveGameInstaceID(GameData.GameID);
		//SaveGameInstaceID(GameData.GameID);
	}
	else
	{
		UE_LOG(LogClass, Warning, TEXT("RequsetGameDataByGameID Failed"));
	}
	
}
	

void UHttpRequest::SaveGameInstaceID(int64 CurrentGameID)
{
	FString SaveFileName = FString::Printf(TEXT("%lld"), CurrentGameID);
	FString Data;

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Data);

	Writer->WriteObjectStart();
		Writer->WriteObjectStart("GameData");
			Writer->WriteValue<int64>(TEXT("CreationTime"), GameData.CreationTime);
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

				Writer->WriteValue<int>(TEXT("ChampId"), GameData.SummonerData[i].ChampionID);
				Writer->WriteValue<int>(TEXT("TeamNumber"), GameData.SummonerData[i].TeamNumber);
				Writer->WriteValue<bool>(TEXT("isWin"), GameData.SummonerData[i].bIsWin);
				Writer->WriteValue<FString>(TEXT("UserName"), GameData.SummonerData[i].UserName);

			Writer->WriteObjectEnd();

	}
		Writer->WriteObjectEnd();
	Writer->WriteObjectEnd();
	
	Writer->Close();


	bool SaveSuccess = FFileHelper::SaveStringToFile(*Data, *(FPaths::ProjectDir() + TEXT("GameData/") + SaveFileName + TEXT(".Json") ));
	if (SaveSuccess)
	{
		UE_LOG(LogClass, Warning, TEXT("SaveFile Success"));
	}
	else
	{
		UE_LOG(LogClass, Warning, TEXT("SaveFile Failed"));
	}
	
}

bool UHttpRequest::CheckExistGame(int64 CurrentGameID)
{
	FString SaveFileName = FString::Printf(TEXT("%lld"), CurrentGameID) + TEXT(".json");

	IFileManager& FileManager = IFileManager::Get();

	TArray<FString> FoundFiles;
	FString TempPath = (FPaths::ProjectDir() + TEXT("GameData/"));
	const TCHAR* RootPath = *TempPath;
	const TCHAR* Extension = _T("*.json");

	FileManager.FindFiles(FoundFiles, RootPath, Extension);

	for (int i = 0; i < FoundFiles.Num(); i++)
	{
		if (FoundFiles[i] == SaveFileName)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	return false;
}

bool UHttpRequest::LoadLocalGameData(int64 CurrentGameID)
{
	
	bool bLoadSuccess = false;

	IFileManager* FileManager = &IFileManager::Get();
	TArray<FString> FoundFiles;
	FString TargetDir = FPaths::ProjectDir() + TEXT("GameData/");
	FString FileExtension = TEXT(".Json");
	FileManager->FindFiles(FoundFiles, *TargetDir, *FileExtension);

	
	if (FoundFiles.FindByKey(CurrentGameID + TEXT(".Json")) != nullptr)
	{
		FString CurrentGameData;
		bLoadSuccess = FFileHelper::LoadFileToString(CurrentGameData, *TargetDir);

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CurrentGameData);
		TSharedPtr<FJsonObject> JsonObject;

		//Deserialize the json data given Reader and the actual object to deserialize
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{	// if Player is not Out clanMember . Do not Save user data

			
			GameData.CreationTime = JsonObject->GetObjectField(TEXT("GameData"))->GetNumberField(TEXT("CreationTime"));
			GameData.GameDuration = JsonObject->GetObjectField(TEXT("GameData"))->GetNumberField(TEXT("GameDuration"));
			GameData.WinningTeamNumber = JsonObject->GetObjectField(TEXT("GameData"))->GetNumberField(TEXT("WinningTeam"));
			GameData.ClanMemberNum = JsonObject->GetObjectField(TEXT("GameData"))->GetNumberField(TEXT("ClanMemberNum"));
			GameData.bIsClanGame = JsonObject->GetObjectField(TEXT("GameData"))->GetBoolField(TEXT("IsClanGame"));
			GameData.GameMode = JsonObject->GetObjectField(TEXT("GameData"))->GetStringField(TEXT("GameMode"));
			GameData.GameType = JsonObject->GetObjectField(TEXT("GameData"))->GetStringField(TEXT("GameType"));
			GameData.QueueId = JsonObject->GetObjectField(TEXT("GameData"))->GetNumberField(TEXT("QueueId"));

			for (int i = 0; i < 10; i++)
			{
				GameData.SummonerData[i].ChampionID = JsonObject->GetObjectField(TEXT("SummonerData"))->GetObjectField(TEXT("Player" +  FString::FromInt(i + 1)))->GetNumberField(TEXT("ChampId"));
				GameData.SummonerData[i].TeamNumber = JsonObject->GetObjectField(TEXT("SummonerData"))->GetObjectField(TEXT("Player" +  FString::FromInt(i + 1)))->GetNumberField(TEXT("TeamNumber"));
				GameData.SummonerData[i].bIsWin = JsonObject->GetObjectField(TEXT("SummonerData"))->GetObjectField(TEXT("Player" +  FString::FromInt(i + 1)))->GetBoolField(TEXT("isWin"));
				GameData.SummonerData[i].UserName = JsonObject->GetObjectField(TEXT("SummonerData"))->GetObjectField(TEXT("Player" +  FString::FromInt(i + 1)))->GetStringField(TEXT("UserName"));
			}
		
		}


		LoadedGameData.Add(GameData);
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
		bLoadSuccess = FFileHelper::LoadFileToString(UserData, *FilePath);

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
		}
	}

	return bLoadSuccess;

}
void UHttpRequest::RequsetPngbyChampId(int64 ChampId)
{
	TSharedPtr<IHttpRequest> HttpRequest = HttpModule->CreateRequest();

	//FString ChampionId = ChampId;

	FString URL = ChampPngAsset + TEXT("champion1.png"); // TEXT("Aatrox") + TEXT(".Png");

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(URL);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UHttpRequest::OnResponseReceivedPngbyChampId);
	HttpRequest->ProcessRequest();
}

void UHttpRequest::OnResponseReceivedPngbyChampId(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{

	FString ImageAssetPath = TEXT("ChampImage/");
	FString BasePath = FPaths::ProjectDir() + ImageAssetPath;
	FString FileSavePath = BasePath + TEXT("Aatrox1.png");		// 경로를 지정 한다.


	// 해당 플렛폼에 맞는 File 클래스 만들기.
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Dir tree 를 만든다.
	//PlatformFile.CreateDirectoryTree(*BasePath);
	IFileHandle* FileHandler = PlatformFile.OpenWrite(*FileSavePath);

	if (FileHandler) 
	{
		// 파일을 새로 쓴다.
		FileHandler->Write(Response->GetContent().GetData(), Response->GetContentLength());
		FileHandler->Flush();		// 저장.
	}

	delete FileHandler;
}