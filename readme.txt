Program Syntax:

AddTempAdmin.exe [username]
AddTempAdmin.exe [username] [hours]

Examples:
AddTempAdmin.exe "test"                 		//Adds local user, test, to admin group for 24 hours
AddTempAdmin.exe "test" "4"						//Adds local user, test, to admin group for 4 hours

AddTempAdmin.exe "AzureAD\test@test.com"		//Adds Azure AD user, test@test.com, to admin group for 24 hours
AddTempAdmin.exe "Domain\test"					//Adds local domain user, domain\test, to admin group for 24 hours

AddTempAdmin.exe "AzureAD\test@test.com" "48"	//Adds Azure AD user, test@test.com, to admin group for 48 hours
AddTempAdmin.exe "Domain\test" "48"				//Adds local domain user, domain\test, to admin group for 48 hours

Important notes:
1. The progam must be run with administrator permissions or as the LocalSystem account.
2. You cannot pass a fraction/decimal as the hours argument.
3. If the system is not booted up for a month after the user is scheduled to be removed from the administrators group, the task will not run anymor
   and will not remove the user. The task will eventually deleted itself after that period.
