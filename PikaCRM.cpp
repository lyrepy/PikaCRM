#include "PikaCRM.h"

#define TFILE <PikaCRM/PikaCRM.t>
#include <Core/t.h>

#define IMAGECLASS SrcImages					// Adding Graphic
#define	IMAGEFILE <PikaCRM/SrcImages.iml>		//
#include <Draw/iml.h>							//

#define TOPICFILE <PikaCRM/srcdoc.tpp/all.i>	// Adding QTF for splash (and for other aims)
#include <Core/topic_group.h>					//


PikaCRM::PikaCRM()
{
	mLanguage=LNG_('Z','H','T','W');
	//mLanguage=LNG_('E','N','U','S');
	Upp::SetLanguage(mLanguage);

	Upp::CtrlLayout(MainFrom, t_("Pika Customer Relationship Management"));
	
	int QtfHigh=20;
	mSplash.SplashInit("PikaCRM/srcdoc/Splash",QtfHigh,getLangLogo(),SrcImages::Logo(),mLanguage);
	
	//WithSplashLayout<TopWindow> splash;
	//CtrlLayout(splash, t_("Application Setup"));
	//splash.OpenMain();
	//Splash();
	
	//splash.Close();
	MainFrom.WhenClose=THISBACK(CloseMainFrom);
	//MainFrom.lbOne=t_("Setting");
}

PikaCRM::~PikaCRM()
{
}


//application control-----------------------------------------------------------
String PikaCRM::GetLogPath()
{
	return getConfigDirPath()+FILE_LOG;	
}
void PikaCRM::OpenMainFrom()
{
	mSplash.ShowSplash();
	String config_file_path = getConfigDirPath()+FILE_CONFIG;	
	String database_file_path = getConfigDirPath()+FILE_DATABASE;
	
	mSplash.ShowSplashStatus(t_("Loading Settings..."));
	SysLog.Info(t_("Loading Settings..."))<<"\n";
	LoadConfig(config_file_path);///@todo see if encrypt?

	if(mConfig.IsDBEncrypt)
	{
		mSplash.HideSplash();
		if(mConfig.IsRememberPW)
		{
			SysLog.Debug("Remeber the PW\n");
			String key=CombineKey(GetSystemKey(), mConfig.Password);
			SysLog.Debug("key:"+key+"\n");
			//PromptOK(key);
			if(mConfig.SystemPWKey.IsEmpty() || key!=mConfig.SystemPWKey)//use different PC
				InputPWCheck();
			/*	else
				;//just using mConfig.Password;
			*/
		}
		else//not Remember PW 
		{
			InputPWCheck();
		}
		mSplash.ShowSplash();
	}

	
	mSplash.ShowSplashStatus(t_("Loading Database..."));
	SysLog.Info(t_("Loading Database..."))<<"\n";
	if(IsHaveDBFile(database_file_path))
	{
		CreateOrOpenDB(database_file_path);//OpenDB
		/*if(GetDBVersion()<DATABASE_VERSION)
			UpdateDB();
		else if(GetDBVersion()>DATABASE_VERSION)
			show can not up compatibility, please use the Latest version
		
		
		
		*/
	}
	else
	{
		mSplash.HideSplash();
		///@todo first welcome
		SetupDB(config_file_path);
		mSplash.ShowSplash();
		CreateOrOpenDB(database_file_path);//CreateDB
		InitialDB();
	}
	/*
	if(IsDBWork())
		//donothing
	else
	{
		//pw error or not the file?
		showErrorMsg();
		goto enter PW
	}
	
	
	*/
	
	MainFrom.OpenMain();
	
	mSplash.ShowSplashStatus(t_("Normal Running..."));
	SysLog.Info(t_("Normal Running..."))<<"\n";
	
	mSplash.SetSplashTimer(1500);
	
	//test if database OK
	bool is_sql_ok=mSql->Execute("select * from System;");
	if(is_sql_ok)
		while(mSql->Fetch())//user,ap_ver,sqlite_ver,db_ver
			SysLog.Debug("") << (*mSql)[0]<<", "<<(*mSql)[1]<<", "<<(*mSql)[2]<<", "<<(*mSql)[3]<<"\n";
	else
	{
		SysLog.Error(mSql->GetLastError()+"\n");
	}
}
void PikaCRM::CloseMainFrom()//MainFrom.WhenClose call back
{
	mSplash.~SplashSV();
	MainFrom.Close();
}

bool PikaCRM::IsHaveDBFile(String database_file_path)
{	
	return FileExists(database_file_path);
}

void PikaCRM::CreateOrOpenDB(String database_file_path)
{
	if(!mSqlite3Session.Open(database_file_path))
	{
		SysLog.Error("can't create or open database file: "+database_file_path+"\n");
		///@todo thow 
	}
	SysLog.Debug("create or open database file: "+database_file_path+"\n");
	
#ifdef _DEBUG
	mSqlite3Session.SetTrace();
#endif
	
	if(!mConfig.Password.IsEmpty())
	{
		SysLog.Info("setting database encrypted key.\n");
		if(!mSqlite3Session.SetKey(getSwap1st2ndChar(mConfig.Password)))
		{
			SysLog.Error("sqlite3 set key error\n");
			///@note we dont know how to deal this error, undefine		
		}
	}

	mSql.reset(new Sql(mSqlite3Session));//mSql=mSqlite3Session will error
}
void PikaCRM::InitialDB()
{
	FileIn initial("initial.sql");
	if(!initial)
	{
		SysLog.Error("can't open database initial file: initial.sql\n");
		///@todo thow 
	}
	bool is_sql_ok = SqlPerformScript(mSqlite3Session,initial);
	
	is_sql_ok=mSql->Execute("INSERT INTO System (user,ap_ver,sqlite_ver,db_ver) VALUES (?,?,?,?);",
							"System",SOFTWARE_VERSION,mSqlite3Session.VersionInfo(),DATABASE_VERSION);
							///@todo set user name
}
void PikaCRM::SetupDB(String config_file_path)
{
	WithInitialDBLayout<TopWindow> d;
	CtrlLayoutOK(d, t_("Setup your database"));
	d.esPassword.Password();
	d.esCheckPassword.Password();
	d.optRequire.SetEditable(false);
	d.optPW.WhenAction=THISBACK1(OnOptPWAction, &d);
	d.ok.WhenPush = THISBACK1(CheckPWSame, &d);
	

	String note,note2;
	note<<"[1 "<<t_("Encrypted database can't be read even if someone has the database file.")<<" ]";
	d.rtNoteEncrypted.SetQTF(note);
	note2<<"[1 "<<t_("Important: if you forgot the password, there is no way to access your database.")<<" ]";
	d.rtNoteEncrypted2.SetQTF(note2);
	if(d.Run() == IDOK) {
		if(d.optPW)
		{
			String tempPW=d.esPassword.GetData();
			mConfig.IsDBEncrypt=true;
			mConfig.Password=getMD5(tempPW<<PW_MAGIC_WORD);
			mConfig.IsRememberPW=!(bool)d.optRequire;	
			mConfig.SystemPWKey=CombineKey(GetSystemKey(), mConfig.Password);	
		}

		SaveConfig(config_file_path);
	}
	
	
}
void PikaCRM::OnOptPWAction(WithInitialDBLayout<TopWindow> * d)
{
	if(d->optPW)
	{
		d->esPassword.SetEditable(true);
		d->esPassword.NotNull(true);
		d->esPassword.WantFocus(true);
		d->esCheckPassword.SetEditable(true);
		d->esCheckPassword.NotNull(true);
		d->esCheckPassword.WantFocus(true);
		d->optRequire.SetEditable(true);
	}
	else 
	{
		d->esPassword.SetEditable(false);
		d->esPassword.NotNull(false);
		d->esPassword.WantFocus(false);
		d->esCheckPassword.SetEditable(false);
		d->esCheckPassword.NotNull(false);
		d->esCheckPassword.WantFocus(false);
		d->optRequire.SetEditable(false);
	}
}
void PikaCRM::CheckPWSame(WithInitialDBLayout<TopWindow> * d)
{
	String p1=d->esPassword;
	String p2=d->esCheckPassword;
	if(d->optPW && !p1.IsEqual(p2))
		PromptOK(t_("Re-Enter Password does correspond to the new Password."));
}

void PikaCRM::LoadConfig(String config_file_path)
{
	if(FileExists(config_file_path))
	{
		if(mConfig.Load(config_file_path))
		{
			SysLog.Debug("loaded the config file\n");
		}
		else
		{
			SysLog.Error("load the config file fail\n");
			///@todo thow and renew outside
		}
	}
	else
	{
		SysLog.Info("make a new config file\n");
		mConfig.IsDBEncrypt=false;
		mConfig.Password="";
		mConfig.IsRememberPW=false;
		mConfig.SystemPWKey="";
		SaveConfig(config_file_path);
	}
}
void PikaCRM::SetConfig()
{
}
void PikaCRM::SaveConfig(String config_file_path)
{
	if(mConfig.Save(config_file_path))
	{
		SysLog.Debug("Save the config file\n");
	}
	else
	{
		SysLog.Error("Save config file fail\n");
		///@todo thow can't save
	}
}


void PikaCRM::InputPWCheck()
{
	WithInputPWLayout<TopWindow> d;
	CtrlLayoutOK(d,t_("Pika Customer Relationship Management"));
	d.ok.WhenPush = THISBACK2(CheckPWRight, &d, mConfig.Password);
	d.optRememberPW = mConfig.IsRememberPW;
	if(d.Run() == IDOK) {
		mConfig.IsRememberPW=(bool)d.optRememberPW;
		if(d.optRememberPW) mConfig.SystemPWKey=CombineKey(GetSystemKey(), mConfig.Password);
		SaveConfig(getConfigDirPath()+FILE_CONFIG);
	}
	else
	{
		///@todo exit all
	}
}

void PikaCRM::CheckPWRight(WithInputPWLayout<TopWindow> * d, String & pw)
{
	String p1=d->esPassword;
	String pwMD5=getMD5(p1<<PW_MAGIC_WORD);
	if(!pw.IsEqual(pwMD5))
		PromptOK(t_("The Password is incorrect!"));
}

String PikaCRM::GetSystemKey()
{
	int		pos;
	String 	output;
	String	key;
	
#ifdef PLATFORM_POSIX
	FILE * 	p_process;
	char  	buf[101]={};
	const char	process_cmd[] = "./hdsn 2>&1";
		if ( NULL != (p_process=popen(process_cmd,"r")) ) 
		{
			if ( fread(buf,sizeof(char),100,p_process) > 0) 
			{
				output=buf;
				//printf("printf %s\n",buf);
			}
			pclose(p_process);
		}
		else 
		{
		    //perror("fail to execute hdsn");
		    output="fail to execute hdsn";
		}

#elif defined(PLATFORM_WIN32)
	///@todo include SystemInfo

#endif

	if( -1 != (pos=output.Find("serial_no:")) )
		key=getMD5(output);
	else
		SysLog.Error(output+"\n");///@remark throw error?
	
	
	return key;
}

String PikaCRM::CombineKey(String key1, String key2) //avoid hacker copy system key and run
{
	String temp;
	if(!key1.IsEmpty() && !key2.IsEmpty())
	{
		key1.Cat(key2);
		temp=getMD5(key1);
	}
	return temp;
}
//end application control-----------------------------------------------------------
//interactive with GUI==============================================================



//end interactive with GUI==========================================================
//private utility-------------------------------------------------------------------
String PikaCRM::getConfigDirPath()
{
	String directory_path(APP_CONFIG_DIR);
#ifdef PLATFORM_POSIX
	String full_directory_path = GetHomeDirFile(directory_path+"/");
#endif

#ifdef PLATFORM_WIN32
	String full_directory_path = GetHomeDirFile("Application Data/"+directory_path+"/");
	//win_full_directory_path=WinPath(full_directory_path);///@todo test if we need do it 
#endif

	if(DirectoryExists(full_directory_path))
		;//do nothing
	else
	{
		if(RealizeDirectory(full_directory_path))
			;//do nothing
		else
		{
			RLOG("can't create the application config directory!");
			//throw this error///@todo throw "Can't create config directory!"
		}
	}

	return full_directory_path;
}
String PikaCRM::getLang4Char()
{
	String lang4=LNGAsText(mLanguage);//EN-US
	if(!lang4.IsEmpty()) lang4.Remove(2,1); //remove "-"
	return lang4;
}
Image PikaCRM::getLangLogo()
{
	String logo_name;
	logo_name="Logo"+getLang4Char();//LogoENUS, LogoZHTW, ...
	int id=SrcImages::Find(logo_name);
							
	Image image;							
	if(id>=0)
		image=SrcImages::Get(id);//SrcImages::LogoENUS()
	else
		image=SrcImages::Logo();
							
	return image;
}
String PikaCRM::getMD5(String & text)
{
	unsigned char digest[16];
	MD5(digest, ~text, text.GetLength());
	String md5ed(digest, 16);
		
	String r;
	for(int i = 0; i < md5ed.GetCount(); i++)
		r << UPP::FormatIntHex((byte)md5ed[i], 2);
		
	return r;
}
String PikaCRM::getSwap1st2ndChar(String & text)
{
	String r(text);
	//String r2=text;
	r.Remove(0);
	r.Insert(1,text[0]);
	
	return r;
}
	    
//end private utility---------------------------------------------------------------