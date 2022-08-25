#include "GowImporterCommandlet.h"
#include "GowImporterCommon.h"

UGowImporterCommandlet::UGowImporterCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

UGowImporterCommandlet::~UGowImporterCommandlet()
{
}

int32 UGowImporterCommandlet::Main(const FString& Params)
{
    UE_LOG(LogGowImporterPlugin, Display, TEXT("Gow Importer Start!"));

    return 0;
}
