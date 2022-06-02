#include "Includes.h"

//local function declarations
void printNetAPIStatus(NET_API_STATUS& status);
void printHelp();
bool createRemoveScript(const std::wstring& userName);
std::string wstrToStr(const std::wstring& wstr);
short setRemoveTask(const std::wstring& userName,const unsigned short hrs);
wchar_t* getDateTimeStrW(const unsigned short hrs);
std::wstring removeSlash(std::wstring userName);

//global constants
const wchar_t efnDirPath[]{L"C:\\ProgramData\\EFNScripts"};
const wchar_t scriptFilePath[]{L"C:\\ProgramData\\EFNScripts\\removeTempAdmin.cmd"};
const std::wstring rootTaskFolder{L'\\'};//Task Scheduler root folder
const std::wstring customTaskFolder{L"\\EFNTasks"};//Task Scheduler custom folder
const std::wstring baseTaskName{L"RemoveTempAdmin "};//Base task name

//main function
int wmain(int argc, wchar_t const* argv[]){
    //Check for the correct number of arguments and abort immediately if incorrect
    if(argc!=2 and argc!=3){printHelp();return 0;}
    
    //Define the local group, user, hours
    const wchar_t localGroup[]{LOCAL_GROUP};
    const std::wstring userName{argv[1]};
    unsigned short hrsUntilRemoval;
    if (argc==2)hrsUntilRemoval = DEFAULT_HOURS_UNTIL_REMOVAL;
    else {
        std::wstring hrsUntilRemovalStr{argv[2]};
        hrsUntilRemoval = static_cast<unsigned short>(std::stoul(hrsUntilRemovalStr));
    }
    
    //Create the removal script file
    if(!(createRemoveScript(userName))){std::cout<<"Failed to create script file."<<std::endl;return 1;}
    else std::cout<<"Successfully created script file.\n";
    
    //Set task to run script at hrsUntilRemoval
    short returnVal{setRemoveTask(userName,hrsUntilRemoval)};
    if(returnVal!=1){
        std::cout<<"\nError creating task. Deleting script file and aborting."<<std::endl;
        std::filesystem::remove(scriptFilePath);
        return 1;
    }
    std::cout<<"\nSet remove task succesffuly."<<std::endl;
    
    //Adding the user to the local group
    std::wstring userNameVar{userName};
    LOCALGROUP_MEMBERS_INFO_3 memberAddRemove{userNameVar.data()};
    LPBYTE buffByte{reinterpret_cast<LPBYTE>(&memberAddRemove)};
    NET_API_STATUS returnValue{NetLocalGroupAddMembers(NULL,localGroup,3,buffByte,1)};

    printNetAPIStatus(returnValue);std::cout.flush();
    return 0;
}

//local function definitions
short setRemoveTask(const std::wstring& userName,const unsigned short hrs){
    //  ------------------------------------------------------
    //  Initialize COM.
    HRESULT hr {CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE)};
    if(FAILED(hr)){
        std::cout<<"\nCoInitializeEx failed: "<<hr;
        return -2;
    }

        //  Set general COM security levels.
    hr = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        0,
        NULL
    );
    if(FAILED(hr)){
        std::cout<<"\nCoInitializeSecurity failed: "<<hr;
        CoUninitialize();
        return -3;
    }
    
    //  Create an instance of the Task Service. 
    ITaskService* pService {nullptr};
    hr = CoCreateInstance( 
        CLSID_TaskScheduler,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_ITaskService,
        reinterpret_cast<void**>(&pService) 
    );  
    if (FAILED(hr)){
        std::cout<<"\nFailed to create an instance of ITaskService: "<<hr;
        CoUninitialize();
        return -4;
    }
        
    //  Connect to the task service.
    hr = pService->Connect(_variant_t(), _variant_t(),_variant_t(), _variant_t());
    if(FAILED(hr)){
        std::cout<<"\nITaskService::Connect failed: "<<hr;
        pService->Release();
        CoUninitialize();
        return -5;
    }

    //  Get the pointer to the root/custom task folder. The custom task folder will hold the
    //  new task that is registered.
    ITaskFolder* pRootFolder {nullptr};
    ITaskFolder* customFolder {nullptr};
    hr = pService->GetFolder(_bstr_t(customTaskFolder.data()),&customFolder);
    if(FAILED(hr)){
        hr = pService->GetFolder(_bstr_t(rootTaskFolder.data()),&pRootFolder);
        if(FAILED(hr)){
            std::cout<<"\nCannot get root folder: "<<hr;
            pService->Release();
            CoUninitialize();
            return -6;
        }

        BSTR pSddl;//will hold the sddl of root ts folder and then apply it to the new subfolder
        SECURITY_INFORMATION securityInfo{};
        pRootFolder->GetSecurityDescriptor(securityInfo,&pSddl);
        hr = pRootFolder->CreateFolder(_bstr_t(customTaskFolder.data()),variant_t(pSddl),&customFolder);
        if(FAILED(hr)){
            std::cout<<"\nCannot create custom folder: "<<hr;
            pRootFolder->Release();
            pService->Release();
            CoUninitialize();
            return -7;
        }
        pRootFolder->Release();                
    }
    
    //  If the same task exists, remove it.
    std::wstring userNameNoSlash {removeSlash(userName)};
    const std::wstring fullTaskName{baseTaskName+userNameNoSlash};
    customFolder->DeleteTask( _bstr_t(fullTaskName.data()), 0);

    //  Create the task definition object to create the task.
    ITaskDefinition* pTask {nullptr};
    hr = pService->NewTask( 0, &pTask );
    pService->Release();  // COM clean up.  Pointer is no longer used.
    if(FAILED(hr)){
        std::cout<<"\nFailed to CoCreate an instance of the TaskService class: "<<hr;
        customFolder->Release();
        CoUninitialize();
        return -8;
    }

    //  Get the registration info for setting the identification.
    IRegistrationInfo* pRegInfo{nullptr};
    hr = pTask->get_RegistrationInfo( &pRegInfo );
    if(FAILED(hr)){
        std::cout<<"\nCannot get identification pointer: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -9;
    }
    
    std::wstring description{L"Removes temp admin, " +userName+L", at next run time"};
    hr = pRegInfo->put_Description(bstr_t(description.data()));
    pRegInfo->Release();
    if(FAILED(hr)){
        std::cout<<"\nFailed to set user as description: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -10;
    }

    //  Create the principal for the task - these credentials
    //  are overwritten with the credentials passed to RegisterTaskDefinition
    IPrincipal* pPrincipal{nullptr};
    hr = pTask->get_Principal( &pPrincipal );
    if(FAILED(hr)){
        std::cout<<"\nCannot get principal pointer: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -12;
    }
    
    //  Set up principal logon type to interactive logon
    hr = pPrincipal->put_LogonType(TASK_LOGON_SERVICE_ACCOUNT);
    if(FAILED(hr)){
        std::cout<<"\nCannot put principal info: "<<hr;
        pPrincipal->Release();
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -13;
    }

    hr = pPrincipal->put_UserId(bstr_t(L"SYSTEM"));
    pPrincipal->Release();
    if(FAILED(hr)){
        std::cout<<"\nCannot put principal info: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -13;
    }

    //  Create the settings for the task
    ITaskSettings* pSettings{nullptr};
    hr = pTask->get_Settings( &pSettings );
    if(FAILED(hr)){
        std::cout<<"\nCannot get settings pointer: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -15;
    }
    
    //  Set settings values for the task.  
    hr = pSettings->put_StartWhenAvailable(VARIANT_TRUE);
    if(FAILED(hr)){
        std::cout<<"\nCannot put setting information: "<<hr;
        customFolder->Release();
        pSettings->Release();
        pTask->Release();
        CoUninitialize();
        return -16;
    }

    hr = pSettings->put_AllowDemandStart(VARIANT_FALSE);
    if(FAILED(hr)){
        std::cout<<"\nCannot put setting information: "<<hr;
        customFolder->Release();
        pSettings->Release();
        pTask->Release();
        CoUninitialize();
        return -16;
    }

    hr = pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
    if(FAILED(hr)){
        std::cout<<"\nCannot put setting information: "<<hr;
        pSettings->Release();
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -16;
    }

    hr = pSettings->put_Compatibility(TASK_COMPATIBILITY_V2_4);
    if(FAILED(hr)){
        std::cout<<"\nCannot put setting information: "<<hr;
        customFolder->Release();
        pSettings->Release();
        pTask->Release();
        CoUninitialize();
        return -16;
    }

    hr = pSettings->put_DeleteExpiredTaskAfter(bstr_t(L"PT10M"));
    pSettings->Release();
    if(FAILED(hr)){
        std::cout<<"\nCannot put setting information: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -16;
    }
    

    // Set the idle settings for the task.
    IIdleSettings *pIdleSettings{nullptr};
    hr = pSettings->get_IdleSettings( &pIdleSettings );
    pSettings->Release();
    if(FAILED(hr)){
        std::cout<<"\nCannot get idle setting information: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -17;
    }

    hr = pIdleSettings->put_WaitTimeout(_bstr_t(L"PT5M"));
    pIdleSettings->Release();
    if(FAILED(hr)){
        std::cout<<"\nCannot put idle setting information: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -18;
    }

    //  Get the trigger collection to insert the time trigger.
    ITriggerCollection* pTriggerCollection {nullptr};
    hr = pTask->get_Triggers( &pTriggerCollection );
    if(FAILED(hr)){
        std::cout<<"\nCannot get trigger collection: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -19;
    }

    //  Add the time trigger to the task.
    ITrigger* pTrigger {nullptr};    
    hr = pTriggerCollection->Create( TASK_TRIGGER_TIME, &pTrigger );     
    pTriggerCollection->Release();
    if(FAILED(hr)){
        std::cout<<"\nCannot create trigger: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -20;
    }

    ITimeTrigger* pTimeTrigger{nullptr};
    hr = pTrigger->QueryInterface(IID_ITimeTrigger, reinterpret_cast<void**>(&pTimeTrigger));
    pTrigger->Release();
    if(FAILED(hr)){
        std::cout<<"\nQueryInterface call failed for ITimeTrigger: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -21;
    }

    hr = pTimeTrigger->put_Id(_bstr_t(L"Trigger1"));
    if(FAILED(hr)){
        std::cout<<"\nCannot put trigger ID: "<<hr;
    }
    
    //  Set the task to start at a certain time. The time 
    //  format should be YYYY-MM-DDTHH:MM:SS(+-)(timezone).
    wchar_t* dateTimeStrW{getDateTimeStrW(hrs)};
    hr = pTimeTrigger->put_StartBoundary(_bstr_t(dateTimeStrW));
    delete [] dateTimeStrW;
    if(FAILED(hr)){
        std::cout<<"\nCannot add start boundary to trigger: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -22;
    }
    // Set an end boundary so it expires so we can delete task after it expires
    const unsigned short endBoundaryHrs{static_cast<const unsigned short>(hrs+HOURS_IN_MONTH)};
    wchar_t* endBoundaryStrW{getDateTimeStrW(endBoundaryHrs)};
    hr = pTimeTrigger->put_EndBoundary(_bstr_t(endBoundaryStrW));
    pTimeTrigger->Release();
    delete [] endBoundaryStrW;
    if(FAILED(hr)){
        std::cout<<"\nCannot add end boundary to trigger: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -23;
    }

    //  Add an action to the task. This task will execute the cmd script.     
    IActionCollection* pActionCollection{nullptr};
    hr = pTask->get_Actions( &pActionCollection );//  Get the task action collection pointer.
    if(FAILED(hr)){
        std::cout<<"\nCannot get Task collection pointer: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -24;
    }

    //  Create the action, specifying that it is an executable action.
    IAction* pAction{nullptr};
    hr = pActionCollection->Create( TASK_ACTION_EXEC, &pAction );
    pActionCollection->Release();
    if(FAILED(hr)){
        std::cout<<"\nCannot create the action: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -25;
    }

    IExecAction* pExecAction{nullptr};
    //  Query interface for the executable task pointer.
    hr = pAction->QueryInterface(IID_IExecAction, reinterpret_cast<void**>(&pExecAction));
    pAction->Release();
    if(FAILED(hr)){
        std::cout<<"\nQueryInterface call failed for IExecAction: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -26;
    }

    //  Set the path of the cmd script
    hr = pExecAction->put_Path(_bstr_t(scriptFilePath));
    pExecAction->Release();
    if(FAILED(hr)){
        std::cout<<"\nCannot put action path: "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -27;
    }

    //  Save the task in the custom folder.
    IRegisteredTask* pRegisteredTask{nullptr};
    hr = customFolder->RegisterTaskDefinition(
            _bstr_t(fullTaskName.data()),
            pTask,
            TASK_CREATE_OR_UPDATE, 
            _variant_t(), 
            _variant_t(),
            TASK_LOGON_SERVICE_ACCOUNT,
            _variant_t(L""),
            &pRegisteredTask
    );
    if(FAILED(hr)){
        std::cout<<"\nError saving the Task : "<<hr;
        customFolder->Release();
        pTask->Release();
        CoUninitialize();
        return -29;
    }
    std::cout<<"Task registered.";
    std::cout.flush();

    //  Clean up.
    customFolder->Release();
    pTask->Release();
    pRegisteredTask->Release();
    CoUninitialize();
    return 1;
}

wchar_t* getDateTimeStrW(const unsigned short hrs){
    std::chrono::time_point futureSysTimePoint{std::chrono::system_clock::now()};
	futureSysTimePoint+= std::chrono::hours(hrs);
    std::time_t futureSysTime{std::chrono::system_clock::to_time_t(futureSysTimePoint)};
    std::tm cTime;//calendar time
    localtime_s(&cTime, &futureSysTime);//puts time_t obj into tm obj which holds time as calendar time
    
    //places the tm obj with specific format into string buff, stringBuffO;fails if you try to put it directly into wostringstream 
    std::ostringstream stringBuffO;
    stringBuffO << std::put_time(&cTime, "%Y-%m-%dT%H:%M:%S");
    const std::string dateTimeTemp{stringBuffO.str()};//Move the string in stringBuffO to standard string variable
    wchar_t* dateTimeStrW{new wchar_t[20]{}};
    unsigned short dateTimeStrWIndex{};
    for(unsigned short c{0};c<dateTimeTemp.length();++c){
        dateTimeStrW[c] = dateTimeTemp[c];
    }
    return dateTimeStrW;
}

bool createRemoveScript(const std::wstring& userName){
    std::string userNameStr{wstrToStr(userName)};
    std::filesystem::create_directory(efnDirPath);
    SetFileAttributesW(efnDirPath,FILE_ATTRIBUTE_HIDDEN);
    std::ofstream fileStream{scriptFilePath};
    if(!fileStream.is_open())return 0;
    fileStream<<"net localgroup \""<<LOCAL_GROUP_STR<<"\" \""<<userNameStr<<"\" /delete";
    fileStream.close();
    return 1;
}

std::string wstrToStr(const std::wstring& wstr){
    unsigned short maxSizeStr{30};
    std::string str(maxSizeStr,'\0');
    unsigned short buffIndex{0};
    while((wstr[buffIndex])!=L'\0'){
        str[buffIndex] = wstr[buffIndex];++buffIndex;
    }
    unsigned short numToErase{static_cast<unsigned short>(maxSizeStr-(buffIndex))};
    str.erase(buffIndex,numToErase);
    return str;
}

std::wstring removeSlash(std::wstring userName){
    for(unsigned short c{0};c<userName.length();++c){
        if(userName[c]==L'\\')userName.erase(c,1);
    }
    return userName;
}

void printNetAPIStatus(NET_API_STATUS& status){
    switch (status){
        case NERR_Success:std::cout<<"Add member succeeded.\n";break;
        case NERR_GroupNotFound:std::cout<<"Local group does not exist.\n";break;
        case ERROR_ACCESS_DENIED:std::cout<<"Access Denied.\n";break;
        case ERROR_NO_SUCH_MEMBER:std::cout<<"User does not exist.\n";break;
        case ERROR_MEMBER_IN_ALIAS:std::cout<<"User was already a member of the group.\n";break;
        case ERROR_INVALID_MEMBER:std::cout<<"User type is invalid.\n";break;
        
        default:
            break;
    }
}

void printHelp(){
    char help[]{
R"*(
Command syntax
AddTempAdmin [username] [hours])*"
    };
	std::cout<<help<<'\n'<<'\n';
    std::cout.flush();
}