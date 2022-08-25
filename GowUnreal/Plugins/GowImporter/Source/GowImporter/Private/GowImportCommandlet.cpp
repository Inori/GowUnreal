#include "GowImportCommandlet.h"
#include "GowImporterCommon.h"

#include "GowInterface.h"

UGowImportCommandlet::UGowImportCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;

    m_gowApi = std::make_shared<GowInterface>();
}

UGowImportCommandlet::~UGowImportCommandlet()
{
}

int32 UGowImportCommandlet::Main(const FString& Params)
{
    UE_LOG(LogGowImporterPlugin, Display, TEXT("Gow Importer Start!"));

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    do 
    {
        FString RdcPath = ParamsMap[TEXT("rdc")];
        if (RdcPath.IsEmpty())
        {
            break;
        }

        auto Resources = m_gowApi->extractResources(TCHAR_TO_UTF8(*RdcPath));

        for (const auto& res : Resources)
        {
            UE_LOG(LogGowImporterPlugin, Display, TEXT("Name %s"), res.name.c_str());
        }
        
    } while (false);

    return 0;
}