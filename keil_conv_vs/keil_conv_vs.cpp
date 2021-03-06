// keil_conv_vs.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "tinyxml2.h"
#include <Windows.h>
#include <iostream>
#include <string.h>
#include <vector>
#include <map>
#include <sstream>
#include <direct.h>
#include <fstream>
#include <stdio.h>
#include "Str.h"
using namespace tinyxml2;
using namespace std;

const char *default_keil_path = "C:\\Keil_v5";


tinyxml2::XMLDocument doc;


//字符串分割函数
std::vector<std::string> split(std::string str, std::string pattern)
{
	std::string::size_type pos;
	std::vector<std::string> result;
	str += pattern;//扩展字符串以方便操作
	size_t size = str.size();

	for (size_t i = 0; i < size; i++)
	{
		pos = str.find(pattern, i);
		if (pos < size)
		{
			std::string s = str.substr(i, pos - i);
			result.push_back(s);
			i = pos + pattern.size() - 1;
		}
	}
	return result;
}

int get_basic_uv_info(string proj, map<string, vector<string>>&grp, string& def, vector<string>& incl)
{
	const char* ret_str = NULL;
	if (XML_SUCCESS !=doc.LoadFile(proj.c_str()))
	{
		cout << "打开文件错误  " << proj << endl;
		return -1;
	}

	bool bXmlUtf8  = true;
	auto node_target = doc.FirstChildElement("Project")->FirstChildElement("Targets")->FirstChildElement("Target");
	auto nodeVariousControls = node_target->FirstChildElement("TargetOption")->
		FirstChildElement("TargetArmAds")->FirstChildElement("Cads")->FirstChildElement("VariousControls");

	auto node_define = nodeVariousControls->FirstChildElement("Define")->ToElement();
	ret_str = node_define->GetText();
	if (ret_str)
		def = AStr(ret_str, true).toAnsi();


	auto node_include = nodeVariousControls->FirstChildElement("IncludePath")->ToElement();
	ret_str = node_include->GetText();
	stringstream stream_inc;
	stream_inc << "..\\;";
	if (ret_str)
	{
		auto vec_inc = split(ret_str, ";");
		for (auto &incs : vec_inc)
		{
			if (incs.length() < 2)
				continue;

			int idx;
			while (incs[0] == ' ')
				incs.erase(0, 1);
			while ((idx = incs.find('/')) != string::npos)	//替换所有的"/"文件符号
				incs.replace(idx, 1, "\\");

			if (incs.c_str()[0] == '.')
			{
				if (incs.c_str()[1] == '.')
					stream_inc << "..\\" << incs;
				else
					stream_inc << "." << incs;
			}
			else if (incs.c_str()[0] == '\\')
				stream_inc << ".." << incs;
			else
				stream_inc << "..\\" << incs;
			stream_inc << ";";
			incl.emplace_back(incs);
		}
	}



	auto node_grp = node_target->FirstChildElement("Groups");
	for (auto ele_group = node_grp->FirstChild();ele_group;ele_group = ele_group->NextSibling())
	{
		string grp_name = AStr(ele_group->FirstChildElement("GroupName")->ToElement()->GetText(),true).toAnsi();
		grp[grp_name] = vector<string>();
		auto node_files = ele_group->FirstChildElement("Files");
		if (!node_files)
			continue;
		for (auto file = node_files->FirstChild();file;file = file->NextSibling())
		{
			string str = AStr(file->FirstChildElement("FilePath")->ToElement()->GetText(), true).toAnsi();
			if (str[str.length() - 1] != 'h' || str[str.length() - 2] != '.')		//排除*.h文件
			{
				string str_to_write;
				int idx;
				while ((idx = str.find('/')) != string::npos)	//替换所有的"/"符号
					str.replace(idx, 1, "\\");

				if (str.c_str()[0] == '.')	// ./或 .\开始
				{
					if (str.c_str()[1] == '\\')
						str_to_write = string(".") + str;
					else if (str.c_str()[1] == '.')
						str_to_write = string("..\\") + str;
					else
						cout << "存在错误路径: " << str << endl;
				}
				else if (str.c_str()[0] == '\\')		//斜杠开始
					str_to_write = string("..") + str;
				else//单独文件夹名开始
					str_to_write = string("..\\") + str;
				grp[grp_name].push_back(str_to_write);
			}

		}
		
	}

	auto node_ret = doc.FirstChildElement("Project")->FirstChildElement("RTE");
	if (!node_ret)
		return 0;

	auto components = node_ret->FirstChildElement("files");
	if (!components)
		return 0;
	
	for (auto component = components->FirstChild();component;component = component->NextSibling() )
	{
		auto instance = component->FirstChildElement("instance");
		if (!instance)
			continue;
		string path = instance->GetText();

		auto package = component->FirstChildElement("component");
		if (!package)
			continue;
		auto grp_name = string("::") + package->Attribute("Cclass");

		grp[grp_name].emplace_back(string("..\\") + path);		
		
	}
	
	return 0;
}

/************************************
* @brief	: 解析dep文件
* @retval	: int - 
* @param	: string uvproj_path - 
* @param	: string proj_dir_path - 
* @param	: string uv_proj_name - 
* @param	: vector<string> incl - 
* @note		: - 
************************************/
int get_dep_info(string uvproj_path, string proj_dir_path, string uv_proj_name, vector<string>& incl)
{
	auto target = doc.FirstChildElement("Project")->FirstChildElement("Targets")
		->FirstChildElement("Target");
	string dep_dir = string(target->FirstChildElement("TargetOption")->FirstChildElement("TargetCommonOption")
		->FirstChildElement("OutputDirectory")->ToElement()->GetText());
	if (dep_dir[0] == '.' && dep_dir[1] == '\\')	//判断相对定位
		dep_dir = dep_dir.substr(2);
	string dep_path = proj_dir_path + dep_dir;		//dep文件目录

	string tar_name = string(AStr(target->FirstChildElement("TargetName")->ToElement()->GetText(), true).toAnsi());//工程名

	string dep_name = dep_path + uv_proj_name + "_" + tar_name + ".dep";	//dep文件具体路径

	std::ifstream dep_file;
	dep_file.open(dep_name);
	if (!dep_file)
		return -1;

	string line;

	while (getline(dep_file, line,'\r'))
	{
		if (line.find("-I") == 0)
		{
			line = line.substr(2);
			if (line[0] == '.'  )	//相对定位
			{
				if (line[1] == '.')
					line = string("..\\") + line;
				else
					line = string(".") + line;				
			}
			if (find(incl.begin(),incl.end(),line) == incl.end())
				incl.emplace_back(line);
		}
		//if (line.find("-F") == 0)
		//{
		//}
	}
	dep_file.close();
	return 0;
}

void setdir(char* location)
{
	char tmp;
	char* p = location + strlen(location) - 1;
	while (*p != '\\' && p >= location) p--;
	if (p >= location) {
		tmp = *p;
		*p = '\0';
		string proj_path(location);
		proj_path += "\\VSProj";
		_mkdir(proj_path.c_str());
		SetCurrentDirectory( proj_path.c_str());
		*p = tmp;
	}
}

bool make_dsw_file(const char* project_name)
{
	const char* dsw_content =
		"Microsoft Developer Studio Workspace File, Format Version 6.00\r\n"
		"# WARNING: DO NOT EDIT OR DELETE THIS WORKSPACE FILE!\r\n"
		"\r\n"
		"###############################################################################\r\n"
		"\r\n"
		"Project: \"%s\"=\".\\%s.dsp\" - Package Owner=<4>\r\n"
		"\r\n"
		"Package=<5>\r\n"
		"{{{\r\n"
		"}}}\r\n"
		"\r\n"
		"Package=<4>\r\n"
		"{{{\r\n"
		"}}}\r\n"
		"\r\n"
		"###############################################################################\r\n"
		"\r\n"
		"Global:\r\n"
		"\r\n"
		"Package=<5>\r\n"
		"{{{\r\n"
		"}}}\r\n"
		"\r\n"
		"Package=<3>\r\n"
		"{{{\r\n"
		"}}}\r\n"
		"\r\n"
		"###############################################################################\r\n"
		"\r\n"
		"";

	char dsw[1024] = { 0 };
	string name(project_name);
	name += ".dsw";
	FILE* fp_dsw = fopen(name.c_str(), "wb");
	if (fp_dsw == NULL) {
		cout << "错误:无法打开 " << name << " 用于写!" << endl;
		return false;
	}

	int len = _snprintf(dsw, sizeof(dsw), dsw_content, project_name, project_name);
	if (fwrite(dsw, 1, len, fp_dsw) == len) {
		cout << "已创建 " << name << " 工作空间文件!" << endl;
		fclose(fp_dsw);
		return true;
	}
	else {
		cout << "错误:文件写入错误!" << endl;
		fclose(fp_dsw);
		remove(name.c_str());
		return false;
	}
}

bool make_dsp_file(const char* project_name,  vector<string>& groups,string& define, string& includepath)
{
	const char* dsp_content =
		"# Microsoft Developer Studio Project File - Name=\"%s\" - Package Owner=<4>\r\n"
		"# Microsoft Developer Studio Generated Build File, Format Version 6.00\r\n"
		"# ** DO NOT EDIT **\r\n"
		"\r\n"
		"# TARGTYPE \"Win32 (x86) Console Application\" 0x0103\r\n"
		"\r\n"
		"CFG=%s - Win32 Debug\r\n"
		"!MESSAGE This is not a valid makefile. To build this project using NMAKE,\r\n"
		"!MESSAGE use the Export Makefile command and run\r\n"
		"!MESSAGE \r\n"
		"!MESSAGE NMAKE /f \"%s.mak\".\r\n"
		"!MESSAGE \r\n"
		"!MESSAGE You can specify a configuration when running NMAKE\r\n"
		"!MESSAGE by defining the macro CFG on the command line. For example:\r\n"
		"!MESSAGE \r\n"
		"!MESSAGE NMAKE /f \"%s.mak\" CFG=\"%s - Win32 Debug\"\r\n"
		"!MESSAGE \r\n"
		"!MESSAGE Possible choices for configuration are:\r\n"
		"!MESSAGE \r\n"
		"!MESSAGE \"%s - Win32 Release\" (based on \"Win32 (x86) Console Application\")\r\n"
		"!MESSAGE \"%s - Win32 Debug\" (based on \"Win32 (x86) Console Application\")\r\n"
		"!MESSAGE \r\n" /*在这之前有7个项目名*/
		"\r\n"
		"# Begin Project\r\n"
		"# PROP AllowPerConfigDependencies 0\r\n"
		"# PROP Scc_ProjName \"\"\r\n"
		"# PROP Scc_LocalPath \"\"\r\n"
		"CPP=cl.exe\r\n"
		"RSC=rc.exe\r\n"
		"\r\n"
		"!IF  \"$(CFG)\" == \"%s - Win32 Release\"\r\n" //项目名
		"\r\n"
		"# PROP BASE Use_MFC 0\r\n"
		"# PROP BASE Use_Debug_Libraries 0\r\n"
		"# PROP BASE Output_Dir \"Release\"\r\n"
		"# PROP BASE Intermediate_Dir \"Release\"\r\n"
		"# PROP BASE Target_Dir \"\"\r\n"
		"# PROP Use_MFC 0\r\n"
		"# PROP Use_Debug_Libraries 0\r\n"
		"# PROP Output_Dir \"Release\"\r\n"
		"# PROP Intermediate_Dir \"Release\"\r\n"
		"# PROP Target_Dir \"\"\r\n"
		"# ADD BASE CPP /nologo /W3 /GX /O2 %s /YX /FD /c\r\n" //宏定义:/D \"WIN32\" /D \"NDEBUG\"
		"# ADD CPP /nologo /W3 /GX /O2 %s %s /YX /FD /c\r\n" //目录+宏定义
		"# ADD BASE RSC /l 0x804 /d \"NDEBUG\"\r\n"
		"# ADD RSC /l 0x804 /d \"NDEBUG\"\r\n"
		"BSC32=bscmake.exe\r\n"
		"# ADD BASE BSC32 /nologo\r\n"
		"# ADD BSC32 /nologo\r\n"
		"LINK32=link.exe\r\n"
		"# ADD BASE LINK32 user32.lib /nologo /subsystem:console /machine:I386\r\n"
		"# ADD LINK32 user32.lib /nologo /subsystem:console /machine:I386\r\n"
		"\r\n"
		"!ELSEIF  \"$(CFG)\" == \"%s - Win32 Debug\"\r\n"     //项目名
		"\r\n"
		"# PROP BASE Use_MFC 0\r\n"
		"# PROP BASE Use_Debug_Libraries 1\r\n"
		"# PROP BASE Output_Dir \"Debug\"\r\n"
		"# PROP BASE Intermediate_Dir \"Debug\"\r\n"
		"# PROP BASE Target_Dir \"\"\r\n"
		"# PROP Use_MFC 0\r\n"
		"# PROP Use_Debug_Libraries 1\r\n"
		"# PROP Output_Dir \"Debug\"\r\n"
		"# PROP Intermediate_Dir \"Debug\"\r\n"
		"# PROP Target_Dir \"\"\r\n"
		"# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od %s /YX /FD /GZ  /c\r\n" //宏定义
		"# ADD CPP /nologo /W3 /Gm /GX /ZI /Od %s %s /YX /FD /GZ  /c\r\n" //目录+宏定义
		"# ADD BASE RSC /l 0x804 /d \"_DEBUG\"\r\n"
		"# ADD RSC /l 0x804 /d \"_DEBUG\"\r\n"
		"BSC32=bscmake.exe\r\n"
		"# ADD BASE BSC32 /nologo\r\n"
		"# ADD BSC32 /nologo\r\n"
		"LINK32=link.exe\r\n"
		"# ADD BASE LINK32 user32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept\r\n"
		"# ADD LINK32 user32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept\r\n"
		"\r\n"
		"!ENDIF \r\n" /*前面只有2个项目名*/
		"\r\n"
		"# Begin Target\r\n"
		"\r\n"
		"# Name \"%s - Win32 Release\"\r\n"    //这里有两个
		"# Name \"%s - Win32 Debug\"\r\n"
		;

	char dsp[100 * 1024] = { 0 };
	string name(project_name);
	name += ".dsp";
	FILE* fp_dsp = fopen(name.c_str(), "wb");
	if (fp_dsp == NULL) {
		cout << "错误:无法打开 " << name << " 用于写!" << endl;
		return false;
	}

	// 定义是逗号分隔的
	define = define + ",__CC_ARM" + ",__weak= ";	//添加__CC_ARM宏定义以正确包含stdint.h
	char* zDefine = new char[define.length() + 2];
	memset(zDefine, 0, define.length() + 2);
	strcpy(zDefine, define.c_str());
	for (size_t iii = 0; iii < define.length() + 2; iii++) {
		if (zDefine[iii] == ',') {
			zDefine[iii] = '\0';
		}
	}
	string strDefine("");
	for (const char* pp = zDefine; *pp;) {
		strDefine += "/D \"";
		strDefine += pp;
		strDefine += "\" ";

		while (*pp) pp++;
		pp++;
	}

	// includepath是分号分隔的
	char* zInclude = new char[includepath.length() + 2];
	memset(zInclude, 0, includepath.length() + 2);
	strcpy(zInclude, includepath.c_str());
	for (size_t jjj = 0; jjj < includepath.length() + 2; jjj++) {
		if (zInclude[jjj] == ';') {
			zInclude[jjj] = '\0';
		}
	}

	string strInclude("");
	for (const char* qq = zInclude; *qq;) {
		strInclude += "/I \"";
		strInclude += qq;
		strInclude += "\" ";

		while (*qq) qq++;
		qq++;
	}

	int len = 0;
	len += _snprintf(dsp, sizeof(dsp) - len, dsp_content,
		project_name, project_name, project_name, project_name, project_name, project_name, project_name,

		project_name,
		strDefine.c_str(),
		strInclude.c_str(), strDefine.c_str(),

		project_name,
		strDefine.c_str(),
		strInclude.c_str(), strDefine.c_str(),

		project_name,
		project_name
	);

	delete[] zDefine;
	delete[] zInclude;

	int len_write = 0;
	for (auto & group : groups) {
		len_write = _snprintf(dsp + len, sizeof(dsp) - len, "%s", group.c_str());
		len += len_write;
	}

	len += _snprintf(dsp + len, sizeof(dsp) - len, "%s", "# End Target\r\n# End Project\r\n");

	if (fwrite(dsp, 1, len, fp_dsp) == len) {
		cout << "已创建 " << name << " 项目文件!" << endl;
		fclose(fp_dsp);
	}
	else {
		cout << "错误:文件写入错误!" << endl;
		fclose(fp_dsp);
		remove(name.c_str());
		return false;
	}
	return true;
}


int main()
{
	string dir_path;
	char uvprojx[256] = {};
	char keil_path[256] = {};
	map<string, vector<string>> grp;
	string def;
	vector<string> incl;


	/*获取工程路径*/
	cout << "输入*.uvprojx路径：" << endl;
	while (strlen(uvprojx)<4)
		cin.getline(uvprojx, sizeof(uvprojx));
	cout << "工程名：" << uvprojx << endl;

	///*设置keil 路径*/
	//cout << "输入Keil路径（默认使用" << default_keil_path << ")：" << endl;
	//cin.getline(keil_path, sizeof(uvprojx));
	//if (strlen(keil_path) > 4)
	//	cout << "工程名：" << uvprojx << endl;
	//else
	//	strcpy_s(keil_path,sizeof(keil_path), "C:\\Keil_v5");


	string uvproj_name(uvprojx);	//文件名称，不带后缀
	string uvproj_dir_path;	//文件夹路径
	auto name_offset_start = uvproj_name.find_last_of('\\') + 1;
	uvproj_dir_path = uvproj_name.substr(0, name_offset_start);
	auto name_offset_end = uvproj_name.find_last_of('.');
	uvproj_name = uvproj_name.substr(name_offset_start, name_offset_end - name_offset_start);
	
	setdir(uvprojx);


	if (get_basic_uv_info(string(uvprojx), grp, def, incl) ==0 )
	{
		get_dep_info(string(uvprojx), uvproj_dir_path, uvproj_name, incl);
		if (make_dsw_file(uvproj_name.c_str()))
		{
			vector<string> grps;
			for (const auto& var : grp)
			{
				stringstream grp_stream("");
				grp_stream << "# Begin Group \"" << var.first << "\"\r\n\r\n";
				grp_stream << "# PROP Default_Filter \"\"\r\n";
				for ( const auto&  var2 : var.second)
					grp_stream << "# Begin Source File\r\n\r\nSOURCE=\"" << var2 << "\"\r\n" << "# End Source File\r\n";
				grp_stream << "# End Group\r\n\r\n";
				grps.emplace_back(grp_stream.str());
			}

			string include_stream("");
			for (const auto& var:incl)
				include_stream += var + ";";

			make_dsp_file(uvproj_name.c_str(), grps, def, include_stream);
		}
	}

	
	
	cout << "回车键结束....." << endl;
	cin.get();
	return 0;
}

