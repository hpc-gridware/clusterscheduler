#pragma once
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "basis_types.h"

// clang-format off

#define MSG_UTILBIN_USAGE                    _MESSAGE(57000, _("usage:"))
#define MSG_COMMAND_RUNCOMMANDASUSERNAME_S   _MESSAGE(57001, _("run commandline under " SFN " of given user"))
#define MSG_COMMAND_EXECUTEFAILED_S          _MESSAGE(57002, _("can't execute command " SFQ))
#define MSG_SYSTEM_RESOLVEUSERFAILED_S       _MESSAGE(57003, _("can't resolve username " SFQ))

#define MSG_COMMAND_USAGECHECKPROG           _MESSAGE(57008, _("checkprog { -ppid | pid processname }\n\n if only the option -ppid is given the parent process id (ppid)\n of checkprog is printed to stdout. \n else check the first 8 letters of process basename\n\nexit status: 0 if process was found or ppid was printed\n             1 if process was not found\n             2 if ps program couldn't be spawned"))
#define MSG_COMMAND_USAGEGETPROGS            _MESSAGE(57009, _("getprogs processname\ncheck and list pids of \"processname\"\n\nexit status: 0 if process(es) were found\n             1 if process(es) was not found\n             2 if ps program couldn't be spawned"))
#define MSG_COMMAND_CALLCHECKPROGORGETPROGS  _MESSAGE(57010, _("program must be called as \"checkprog\" or \"getprogs\""))
#define MSG_PROC_PIDNOTRUNNINGORWRONGNAME_IS _MESSAGE(57011, _("pid \"%d\" is not running or has another program name than " SFQ))
#define MSG_PROC_PIDISRUNNINGWITHNAME_IS     _MESSAGE(57012, _("pid \"%d\" with process name " SFQ " is running"))
#define MSG_COMMAND_SPANPSFAILED             _MESSAGE(57013, _("could not spawn ps command"))
#define MSG_COMMAND_RUNPSCMDFAILED_S         _MESSAGE(57014, _("could not run " SFQ " to get pids of processes"))
#define MSG_PROC_FOUNDNOPROCESS_S            _MESSAGE(57015, _("found no running processes with name " SFQ))
#define MSG_PROC_FOUNDPIDSWITHNAME_S         _MESSAGE(57016, _("found the following pids which have process name " SFQ))
#define MSG_COMMAND_SMF_INIT_FAILED          _MESSAGE(57019, _("failed to initialize libraries for SMF support"))

#define MSG_COMMAND_STATFAILED               _MESSAGE(57017, _("stat failed"))
#define MSG_SYSTEM_UIDRESOLVEFAILED          _MESSAGE(57018, _("can't resolve userid"))

#define MSG_SYSTEM_HOSTNAMEIS_S              _MESSAGE(57020, _("Hostname: " SFN))
#define MSG_SYSTEM_ALIASES                   _MESSAGE(57021, _("Aliases:  "))
#define MSG_SYSTEM_ADDRESSES                 _MESSAGE(57022, _("Host Address(es): "))
#define MSG_COMMAND_USAGE_GETHOSTNAME        _MESSAGE(57023, _("get resolved hostname of this host"))

#define MSG_COMMAND_USAGE_GETSERVBYNAME      _MESSAGE(57026, _("get number of a tcp service"))
#define MSG_SYSTEM_SERVICENOTFOUND_S         _MESSAGE(57027, _("service " SFN " not found"))
#define MSG_SYSTEM_RETMEMORYINDICESFAILED    _MESSAGE(57028, _("failed retrieving memory indices"))
#define MSG_SYSTEM_PORTNOTINUSE_S            _MESSAGE(57038, _("port " SFN " not in use"))

#define SGE_INFOTEXT_TESTSTRING_S "Welcome, " SFN "\nhave a nice day!"
#define SGE_INFOTEXT_UNDERLINE  "-"

#define SGE_INFOTEXT_ONLY_ALLOWED_SS                  _MESSAGE(57036, _("There are only two answers allowed: " SFQ " or " SFQ "!"))

#define MSG_SPOOLDEFAULTS_COMMANDINTRO1               _MESSAGE(57100, _("create default entries during installation process"))
#define MSG_SPOOLDEFAULTS_COMMANDINTRO2               _MESSAGE(57101, _("following are the valid commands:"))
#define MSG_SPOOLDEFAULTS_TEST                        _MESSAGE(57102, _("test                          test the spooling framework"))
#define MSG_SPOOLDEFAULTS_MANAGERS                    _MESSAGE(57103, _("managers <mgr1> [<mgr2> ...]  create managers"))
#define MSG_SPOOLDEFAULTS_OPERATORS                   _MESSAGE(57104, _("operators <op1> [<op2> ...]   create operators"))
#define MSG_SPOOLDEFAULTS_PES                         _MESSAGE(57105, _("pes <template_dir>            create parallel environments"))
#define MSG_SPOOLDEFAULTS_CONFIGURATION               _MESSAGE(57106, _("configuration <template>      create the global configuration"))
#define MSG_SPOOLDEFAULTS_LOCAL_CONF                  _MESSAGE(57107, _("local_conf <template> <name>  create a local configuration"))
#define MSG_SPOOLDEFAULTS_USERSETS                    _MESSAGE(57108, _("usersets <template_dir>       create usersets"))
#define MSG_SPOOLDEFAULTS_CANNOTCREATECONTEXT         _MESSAGE(57109, _("cannot create spooling context"))
#define MSG_SPOOLDEFAULTS_CANNOTSTARTUPCONTEXT        _MESSAGE(57110, _("cannot startup spooling context"))
#define MSG_SPOOLDEFAULTS_COMPLEXES                   _MESSAGE(57111, _("complexes <template_dir>      create complexes"))
#define MSG_SPOOLDEFAULTS_ADMINHOSTS                  _MESSAGE(57112, _("adminhosts <template_dir>     create admin hosts"))
#define MSG_SPOOLDEFAULTS_SUBMITHOSTS                 _MESSAGE(57113, _("submithosts <template_dir>    create submit hosts"))
#define MSG_SPOOLDEFAULTS_CALENDARS                   _MESSAGE(57114, _("calendars <template_dir>      create calendars"))
#define MSG_SPOOLDEFAULTS_CKPTS                       _MESSAGE(57115, _("ckpts <template_dir>          create checkpoint environments"))
#define MSG_SPOOLDEFAULTS_EXECHOSTS                   _MESSAGE(57116, _("exechosts <template_dir>      create execution hosts"))
#define MSG_SPOOLDEFAULTS_PROJECTS                    _MESSAGE(57117, _("projects <template_dir>       create projects"))
#define MSG_SPOOLDEFAULTS_CQUEUES                     _MESSAGE(57118, _("cqueues <template_dir>        create cluster queues"))
#define MSG_SPOOLDEFAULTS_USERS                       _MESSAGE(57119, _("users <template_dir>          create users"))
#define MSG_SPOOLDEFAULTS_SHARETREE                   _MESSAGE(57120, _("sharetree <template>          create sharetree"))
#define MSG_SPOOLDEFAULTS_CANTREADGLOBALCONF_S        _MESSAGE(57125, _("couldn't read global config file " SFN))
#define MSG_SPOOLDEFAULTS_CANTREADLOCALCONF_S         _MESSAGE(57126, _("couldn't read local config file " SFN))
#define MSG_SPOOLDEFAULTS_CANTHANDLECLASSICSPOOLING   _MESSAGE(57129, _("can't handle classic spooling"))

#define MSG_SPOOLINIT_COMMANDINTRO0    _MESSAGE(57200, _("method shlib libargs command [args]"))
#define MSG_SPOOLINIT_COMMANDINTRO1    _MESSAGE(57201, _("database maintenance"))
#define MSG_SPOOLINIT_COMMANDINTRO2    _MESSAGE(57202, _("following are the valid commands"))
#define MSG_SPOOLINIT_COMMANDINTRO3    _MESSAGE(57203, _("init [history]    initialize the database [with history enabled]"))
#define MSG_SPOOLINIT_COMMANDINTRO4    _MESSAGE(57204, _("history on|off    switch spooling with history on or off"))
#define MSG_SPOOLINIT_COMMANDINTRO5    _MESSAGE(57205, _("backup path       backup the database to path"))
#define MSG_SPOOLINIT_COMMANDINTRO6    _MESSAGE(57206, _("purge days        remove historical data older than days"))
#define MSG_SPOOLINIT_COMMANDINTRO7    _MESSAGE(57207, _("vacuum            compress database, update statistics"))
#define MSG_SPOOLINIT_COMMANDINTRO8    _MESSAGE(57208, _("info              output information about the database"))
#define MSG_SPOOLINIT_COMMANDINTRO9    _MESSAGE(57209, _("method            output the compiled in spooling method"))

#define MSG_DBSTAT_COMMANDINTRO1       _MESSAGE(57300, _("database query and maintenance"))
#define MSG_DBSTAT_COMMANDINTRO2       _MESSAGE(57301, _("following are the valid commands:"))
#define MSG_DBSTAT_LIST                _MESSAGE(57302, _("list [object type]  list all objects [matching object type]"))
#define MSG_DBSTAT_DUMP                _MESSAGE(57303, _("dump key            dump the object matching key"))
#define MSG_DBSTAT_LOAD                _MESSAGE(57304, _("load key file       load an object from file and store it using key"))
#define MSG_DBSTAT_DELETE              _MESSAGE(57305, _("delete key          delete the object matching key"))
#define MSG_DBSTAT_ERRORUNDUMPING_S    _MESSAGE(57306, _("error reading object from file " SFN))
#define MSG_DBSTAT_INVALIDKEY_S        _MESSAGE(57307, _("invalid key " SFQ))

#define MSG_SUIDROOT_START_BY_NONROOT       _MESSAGE(213109, _(SFN ": must be started with uid != 0"))
#define MSG_SUIDROOT_EFFECTIVE_USER_ROOT    _MESSAGE(213110, _(SFN ": effective uid should be 0"))
#define MSG_SUIDROOT_BIND_PRIV_SOCK_FAILED  _MESSAGE(213111, _(SFN ": binding a privileged socket fails"))

// clang-format on