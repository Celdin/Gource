/*
    Copyright (C) 2009 Andrew Caudwell (acaudwell@gmail.com)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version
    3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "git.h"
#include <iostream>
#include <fstream>
#include "recup-file.h"
#include "../gource_settings.h"

// parse git log entries

//git-log command notes:
// - no single quotes on WIN32 as system call treats them differently
// - 'user:' prefix allows us to quickly tell if the log is the wrong format
//   and try a different format (eg cvs-exp)

std::string GitCommitLog::logCommand() {


    std::string log_command = "git log "
    "--pretty=format:user:%aN%n%ct "
    "--reverse --raw --encoding=UTF-8 "
    "--no-renames "
    "--no-abbrev";

    if(!gGourceSettings.git_branch.empty()) {
        log_command += " ";
        log_command += gGourceSettings.git_branch;
    }

    if(!gGourceSettings.start_date.empty()) {
        log_command += " --since ";
        log_command += gGourceSettings.start_date;
    }

    if(!gGourceSettings.stop_date.empty()) {
        log_command += " --until ";
        log_command += gGourceSettings.stop_date;
    }

    return log_command;
}

GitCommitLog::GitCommitLog(const std::string& logfile) : RCommitLog(logfile, 'u') {
    printf("test\n");

    log_command = logCommand();

    //can generate log from directory
    if(!logf && is_dir) {
        logf = generateLog(logfile);

        if(logf) {
            success  = true;
            seekable = true;
        }
    }
}

BaseLog* GitCommitLog::generateLog(const std::string& dir) {
    //get working directory
    char cwd_buff[1024];

    if(getcwd(cwd_buff, 1024) != cwd_buff) {
        return 0;
    }

    //does directory have a .git ?
    std::string gitdir = dir + std::string("/.git");
    struct stat dirinfo;
    int stat_rc = stat(gitdir.c_str(), &dirinfo);
    if(stat_rc!=0 || !(dirinfo.st_mode & S_IFDIR)) {
        return 0;
    }

    // do we have this client installed
    requireExecutable("git");

    std::string command = getLogCommand();

    //create temp file
    createTempLog();

    if(temp_file.size()==0) return 0;

    if(chdir(dir.c_str()) != 0) {
        return 0;
    }

    char cmd_buff[2048];
    sprintf(cmd_buff, "%s > %s", command.c_str(), temp_file.c_str());

    int command_rc = systemCommand(cmd_buff);

    if(command_rc != 0) {
        chdir(cwd_buff);
        return 0;
    }

    // check for new-enough Git version
    // if %aN does not appear to be supported try %an
    std::ifstream in(temp_file.c_str());
    char firstBytes[9];
    in.read(firstBytes, 8);
    in.close();
    firstBytes[8] = '\0';
    if(!strcmp(firstBytes, "user:%aN")) {
        char *pos = strstr(cmd_buff, "%aN");
        pos[2] = 'n';
        command_rc = systemCommand(cmd_buff);
    }

    //change back to original directoy
    chdir(cwd_buff);

    if(command_rc != 0) {
        return 0;
    }

    BaseLog* seeklog = new SeekLog(temp_file);

    return seeklog;
}

// parse modified git format log entries

bool GitCommitLog::parseCommit(RCommit& commit) {

    std::string line;
    std::string fileName;
    commit.username = "";

    while(logf->getNextLine(line) && line.size()) {

        if(line.find("user:") == 0) {

            //username follows user prefix
            commit.username = line.substr(5);

            if(!logf->getNextLine(line)) return false;

            commit.timestamp = atol(line.c_str());

            //this isnt a commit we are parsing, abort
            if(commit.timestamp == 0) return false;

            continue;
        }

        //should see username before files
        if(commit.username.empty()) return false;

        size_t tab = line.find('\t');

        //incorrect log format
        if(tab == std::string::npos || tab == 0 || tab == line.size()-1) continue;
        std::string maLigne = line;
        maLigne = maLigne.substr(maLigne.find(" ")+1,maLigne.size());
        maLigne = maLigne.substr(maLigne.find(" ")+1,maLigne.size());
        std::string firstCommit = maLigne.substr(0,maLigne.find(" "));
        maLigne = maLigne.substr(maLigne.find(" ")+1,maLigne.size());
        std::string secondCommit = maLigne.substr(0,maLigne.find(" "));
        std::string status = line.substr(tab - 1, 1);
        std::string file   = line.substr(tab + 1);

        if(file.find(".java")!=std::string::npos){
            char maCommande[1024];
            if(firstCommit != "0000000000000000000000000000000000000000" && secondCommit != "0000000000000000000000000000000000000000"){
                sprintf(maCommande,"git diff --unified=3000 %s %s>/tmp/_diff.txt\n",firstCommit.c_str(),secondCommit.c_str());
                system(maCommande);
                fileName="/tmp/_diff.txt";
                std::ifstream fichier(fileName, std::ios::in);

                bool newMethode = false;
                bool ajout = false;
                bool supretion = false;
                RecupFile rf = RecupFile();
                std::string ligne;
                getline(fichier, ligne);
                getline(fichier, ligne);
                getline(fichier, ligne);
                getline(fichier, ligne);
                while(getline(fichier, ligne))  // tant que l'on peut mettre la ligne dans "contenu"
                {
                    if(rf.estUneMethode(ligne.substr(1,ligne.size()))){
                        if(ajout && supretion){
                            //check for and remove double quotes
                            if(file.find('"') == 0 && file.rfind('"') == file.size()-1) {
                                if(file.size()<=2) continue;
                                commit.addFile(file.substr(1,file.size()-2), "M");
                            }else{
                                commit.addFile(file, "M");
                            }
                        }else if (ajout){
                            //check for and remove double quotes
                            if(file.find('"') == 0 && file.rfind('"') == file.size()-1) {
                                if(file.size()<=2) continue;
                                commit.addFile(file.substr(1,file.size()-2), "A");
                            }else{
                                commit.addFile(file, "A");
                            }
                        }else if(supretion){
                            //check for and remove double quotes
                            if(file.find('"') == 0 && file.rfind('"') == file.size()-1) {
                                if(file.size()<=2) continue;
                                commit.addFile(file.substr(1,file.size()-2), "D");
                            }else{
                                commit.addFile(file, "D");
                            }
                        }
                        newMethode=true;
                        ajout = false;
                        supretion = false;
                    }else if(ligne.find("{") != std::string::npos)
                        rf.level++;
                    if(ligne.find("}") != std::string::npos){
                        if(rf.estUneFinMethode()){
                            newMethode=false;
                            if(ajout && supretion){
                                //check for and remove double quotes
                                if(file.find('"') == 0 && file.rfind('"') == file.size()-1) {
                                    if(file.size()<=2) continue;
                                    commit.addFile(file.substr(1,file.size()-2), "M");
                                }else{
                                    commit.addFile(file, "M");
                                }
                            }else if (ajout){
                                //check for and remove double quotes
                                if(file.find('"') == 0 && file.rfind('"') == file.size()-1) {
                                    if(file.size()<=2) continue;
                                    commit.addFile(file.substr(1,file.size()-2), "A");
                                }else{
                                    commit.addFile(file, "A");
                                }
                            }else if(supretion){
                                //check for and remove double quotes
                                if(file.find('"') == 0 && file.rfind('"') == file.size()-1) {
                                    if(file.size()<=2) continue;
                                    commit.addFile(file.substr(1,file.size()-2), "D");
                                }else{
                                    commit.addFile(file, "D");
                                }
                            }
                            newMethode=false;
                            ajout = false;
                            supretion = false;
                        }
                        rf.level--;
                    }
                    if((ligne.c_str())[0]=='+'){
                        ajout = true;
                    }else if((ligne.c_str())[0]=='-'){
                        supretion = true;
                    }
                }

                fichier.close();
            }else{
                //check for and remove double quotes
                if(file.find('"') == 0 && file.rfind('"') == file.size()-1) {
                    if(file.size()<=2) continue;
                    commit.addFile(file.substr(1,file.size()-2), status);
                }else{
                    commit.addFile(file, status);
                }
            }
        }

        if(file.empty()) continue;

        //check for and remove double quotes
        if(file.find('"') == 0 && file.rfind('"') == file.size()-1) {
            if(file.size()<=2) continue;

            file = file.substr(1,file.size()-2);
        }

        commit.addFile(file, status);
    }

    //check we at least got a username
    if(commit.username.empty()) return false;

    return true;
}
