/*=============================================================================
#
#     FileName: mysql_wrapper.cpp
#         Desc: mysql的cpp封装版本
#
#       Author: dantezhu
#        Email: zny2008@gmail.com
#     HomePage: http://www.vimer.cn
#
#      Created: 2011-03-09 19:10:45
#      Version: 0.0.1
#      History:
#               0.0.1 | dantezhu | 2011-03-09 19:10:45 | initialization
#
=============================================================================*/
#include "mysql_wrapper.h"

int CMYSQLWrapper::MapName2Index(MYSQL_RES* result, map<string, int>& mapName2Index)
{
    if (!result)
    {
        return -1;
    }
    MYSQL_FIELD *fields = NULL;

    uint32_t num_fields = mysql_num_fields(result);
    fields = mysql_fetch_fields(result);
    if (!fields)
    {
        return -2;
    }
    for(uint32_t i = 0; i < num_fields; i++)
    {
        //printf("Field %u is %s\n", i, fields[i].name);
        mapName2Index[fields[i].name]=i;
    }
    return 0;
}

CMYSQLWrapper::CMYSQLWrapper()
{
    m_szErrMsg[0]='\0';
    m_Database = NULL;
}
CMYSQLWrapper::~CMYSQLWrapper()
{
    Close();
}

char* CMYSQLWrapper::GetErrMsg()
{
    return m_szErrMsg;
}

void CMYSQLWrapper::_CloseMySQL()
{
    if(m_Database)
    {
        mysql_close(m_Database);
        m_Database = NULL;
    }
}

int CMYSQLWrapper::Connect(const char* ip,const char* user,const char* pwd,const char* db)
{
    _CloseMySQL();

    // Initialize MySQL...
    m_Database = mysql_init(NULL);

    // Failed...
    if(!m_Database)
    {
        MYSQL_WRAPPER_ERROR("Error: Unable to initialize MySQL API\n");

        return EMYSQLErrDBInit;
    }

    /*
    设置重连的官方解释如下:

    MYSQL_OPT_RECONNECT (argument type: my_bool *)

    Enable or disable automatic reconnection to the server if the connection is found to have been lost. 
    Reconnect has been off by default since MySQL 5.0.3; this option is new in 5.0.13 and provides a way 
    to set reconnection behavior explicitly.

    Note: mysql_real_connect() incorrectly reset the MYSQL_OPT_RECONNECT option to its default value 
    before MySQL 5.0.19. Therefore, prior to that version, if you want reconnect to be enabled for each 
    connection, you must call mysql_options() with the MYSQL_OPT_RECONNECT option after each call to 
    mysql_real_connect(). This is not necessary as of 5.0.19: Call mysql_options() only before 
    mysql_real_connect() as usual.*/

#if MYSQL_VERSION_ID >= 50019

    char reconnect = 1;//自动重连的设置

    //set re-conn to true. could use ping to reconn
    if (mysql_options(m_Database, MYSQL_OPT_RECONNECT, &reconnect) != 0) {
        MYSQL_WRAPPER_ERROR("mysql_options Error %u: %s\n",mysql_errno(m_Database), mysql_error(m_Database));
        return EMYSQLErrDBInit;
    }

#endif

    // Connect to server and check for error...
    if(mysql_real_connect(m_Database, ip, user, pwd, db, 0, NULL, 0) < 0)
    {
        // Alert user...
        MYSQL_WRAPPER_ERROR("Error: Unable to connect to server[%s]\n",mysql_error(m_Database));

        return EMYSQLErrDBConn;
    }

#if MYSQL_VERSION_ID >= 50013 && MYSQL_VERSION_ID < 50019

    char reconnect = 1;//自动重连的设置

    //set re-conn to true. could use ping to reconn
    if (mysql_options(m_Database, MYSQL_OPT_RECONNECT, &reconnect) != 0) {
        MYSQL_WRAPPER_ERROR("mysql_options Error %u: %s\n", mysql_errno(m_Database), mysql_error(m_Database));
        return EMYSQLErrDBInit;
    }

#endif
#if MYSQL_VERSION_ID < 50013
#warning "MYSQL_VERSION_ID <= 50013, not support MYSQL_OPT_RECONNECT"
#endif

    return 0;
}
int CMYSQLWrapper::Execute(const char* strSql)
{
    if(m_Database == NULL)
    {
        MYSQL_WRAPPER_ERROR("Error: execute error,pointer is null\n");
        return EMYSQLErrSystemPointer;
    }

    if(mysql_query(m_Database, strSql) != 0)
    {
        MYSQL_WRAPPER_ERROR("Error: Unable to execute query,errMsg=%s\n",mysql_error(m_Database));
        return EMYSQLErrDBExe;
    }
    return 0;
}

int CMYSQLWrapper::Result(MYSQL_RES*& result)
{
    if(m_Database == NULL)
    {
        MYSQL_WRAPPER_ERROR("Error: execute error,pointer is null\n");
        return EMYSQLErrSystemPointer;
    }

    // Retrieve query result from server...
    MYSQL_RES *t_Result = NULL;
    t_Result = mysql_store_result(m_Database);

    // Failed...
    if(!t_Result)
    {
        MYSQL_WRAPPER_ERROR("Error: Unable to retrieve result\n");
        return EMYSQLErrDBRes;
    }
    //对外暴露
    result = t_Result;
    return 0;
}

int CMYSQLWrapper::AffectRows()
{
    int ret = mysql_affected_rows(m_Database);
    if(ret<=0)
    {
        MYSQL_WRAPPER_ERROR("Error: AffectRows rows:[%u]\n",ret);
        return EMYSQLErrDBRes;
    }
    return ret;
}

string CMYSQLWrapper::EscapeRealString(const char* src, uint32_t len)
{
    char *escapeBuff = new char[len*2 + 1];

    mysql_real_escape_string(m_Database,escapeBuff,src,len);
    string strDest(escapeBuff);

    delete [] escapeBuff;
    escapeBuff = NULL;

    return strDest;
}

string CMYSQLWrapper::EscapeRealString(const char* src)
{
    uint32_t len = strlen(src);

    char *escapeBuff = new char[len*2 + 1];

    mysql_real_escape_string(m_Database,escapeBuff,src,len);
    string strDest(escapeBuff);

    delete [] escapeBuff;
    escapeBuff = NULL;

    return strDest;
}

void CMYSQLWrapper::Close()
{
    _CloseMySQL();
}

int CMYSQLWrapper::ExecuteRead(const char* strSql, vector<map<string, MYSQLValue> > &vecData)
{
    int ret = Execute(strSql);
    if (ret)
    {
        return ret;
    }

    MYSQL_RES * result = NULL;
    ret = Result(result);
    if (ret)
    {
        return ret;
    }

    StMYSQLRes freeRes(result);//让自动析构

    uint32_t unRecords = mysql_num_rows(result);

    if (0 == unRecords)
    {
        return 0;
    }

    uint32_t num_fields = mysql_num_fields(result);
    MYSQL_FIELD *fields = NULL;
    fields = mysql_fetch_fields(result);
    if (!fields)
    {
        MYSQL_WRAPPER_ERROR("Error mysql_fetch_fields fail");
        return EMYSQLErrSystem;
    }

    unsigned long *lengths = NULL;
    MYSQL_ROW row;
    for(uint32_t unIndex = 0; unIndex < unRecords; unIndex++)
    {
        row=mysql_fetch_row(result);
        lengths = mysql_fetch_lengths(result);
        if (!lengths)
        {
            MYSQL_WRAPPER_ERROR("Error mysql_fetch_lengths fail,index:%u",unIndex);
            return EMYSQLErrSystem;
        }

        map<string, MYSQLValue> tmpMapData;
        for(uint32_t i = 0; i < num_fields; i++)
        {
            MYSQLValue &value = tmpMapData[fields[i].name];
            value.SetData((char*)row[i], lengths[i]);
        }
        vecData.push_back(tmpMapData);
    }

    return 0;
}
int CMYSQLWrapper::ExecuteWrite(const char* strSql, int& affectRowsCount)
{
    int ret = Execute(strSql);
    if (ret)
    {
        return ret;
    }

    affectRowsCount = AffectRows();

    return 0;
}
