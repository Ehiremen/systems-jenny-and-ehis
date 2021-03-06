//
//  Sniff.cpp
//  Programs
//
//  Created by Ehiremen Ekore on 9/30/20.
//  Copyright © 2020 Ehiremen Ekore. All rights reserved.
//

#include "Sniff.hpp"

Sniff::Sniff(Params& params) : params(params), myOutStream(params.getOutputStream()) {
    params.print();
    
    startingPath = ".";     // we're telling ourselves that the simple name
    currentDirName = ".";   // of the cwd is "."
    
    istringstream searchWords(params.getWords());
    
    string tempWord;
    for(;;) {
        searchWords >> tempWord;
        words.push_back(tempWord);
        if( searchWords.eof() ) break;
     }
    
    // remove duplicates from words vector
    words.resize(distance(words.begin(), unique(words.begin(), words.end())));
    
    // populate all alpha chars
    alphaChars = "";
    for(int k=0; k<26; ++k) { alphaChars += ('a' + k);}
    for(int k=0; k<26; ++k) { alphaChars += ('A' + k);}
    
    cerr << "Sniffer instance successfully created" << endl;
}

// -----------------------------------------------------------------------

void Sniff::print( ostream& out ) {
    if (suspiciousFiles.empty()){
        out << "No suspicious files found" << endl << endl;
    }
    
    else {
        out << endl << endl;
        for (int k=0; k<60; k++) out << "=";
        out << endl << setw(40) << setfill(' ') << "SNIFFER RESULTS\n";
        for (int k=0; k<60; k++) out << "=";
        for (FileID fID: suspiciousFiles) { fID.print(out); }
        out << endl;
    }
}

// -----------------------------------------------------------------------

FileID Sniff::oneFile(struct dirent *currentDirEntry) {
    FileID currentFile = FileID(currentDirEntry->d_name, currentDirEntry->d_ino, startingPath);
    
    ifstream thisFile;
    thisFile.open(currentFile.getName());
    
    if (!thisFile) {
        if (params.isVerbatim()) myOutStream << " (couldn't be opened)";
    }
    
    else if (thisFile.peek() == ifstream::traits_type::eof()) {
        if (params.isVerbatim()) myOutStream << " (is empty file)";
    }
    
    else {
        string currentWord, wordToCheck;
        
        // read file till eof
        while(!thisFile.eof()) {
            thisFile >> currentWord;
            size_t endOfString = currentWord.find_last_of(alphaChars);
            wordToCheck = currentWord.substr(0, endOfString+1);
            
            // compare wordToCheck against all words in words vector
            vector<string> tempFoundWords = searchWord(wordToCheck, params.isCaseSensitive());
            for(string s: tempFoundWords) { currentFile.insertFoundWord(s); }
        }
    }
    
    thisFile.close();
    return currentFile;
}

// -----------------------------------------------------------------------

vector<string> Sniff::searchWord (string word, bool isCaseSensitive) {
    vector<string> out;
    
    if (word.empty()) return out;
    
    if (!isCaseSensitive) { transform(word.begin(), word.end(), word.begin(), ::tolower); }
    
    for (string s: words) {
        // if  not case sensitive, make each string in vector lowercase
        if (!isCaseSensitive) {
            transform(s.begin(), s.end(), s.begin(), ::tolower);
        }
        
        if (word.find(s) != string::npos) out.push_back(s); // word was found
    }
    return out;
}

// -----------------------------------------------------------------------

void Sniff::run(string startingDir) {
    cerr << "\nParam passed: " << startingDir << endl;
    cerr << "\nbefore chdir, cwd: " << getcwd(NULL, 0) << endl;
    
    chdir(startingDir.c_str());
    
    string cwd =getcwd(NULL, 0);
    cerr << "\nafter chdir, cwd: " << cwd << endl;
    
    cerr << "\nstarting sniff..." << endl;
    
    startingPath = ".";     // at this point, "." is the simple name of
    currentDirName = ".";   // whatever directory we are currently in
   
    if (params.isVerbatim()){
        string simpleStartingDir = cwd.substr(cwd.find_last_of("/") + 1);
        myOutStream << endl << simpleStartingDir << " (dir)";
    }
    
    travel(startingPath, currentDirName, 1);
    print(myOutStream);
    
    for (int k=0; k<60; k++) myOutStream << "-"; // mark end of run
    myOutStream << endl << endl;
}

// -----------------------------------------------------------------------

/* Comments:
    * By having DIR* dir local to Sniff::travel, we (lazily) avoid
    * worrying about correctly closing DIR
 */
void Sniff::travel(string path, string nextDir, int depth){
    DIR *dir;
    struct dirent *currentDirEntry;
    
    if ((dir = opendir(path.c_str())) == NULL) {
        myOutStream << "Unable to open dir " << nextDir << endl;
        return;
    }
    
    currentDirName = nextDir;
        // both of these are actually useless variables.
        // nextDir is passed but never used. I can't tell you
        // why currentDirName is still in this class
    
    currentDirEntry = readdir(dir); // discard .
    currentDirEntry = readdir(dir); // discard ..
    
    while ((currentDirEntry = readdir(dir)) != NULL) {
        startingPath = path; // Sniff::oneFile needs this variable to
                             // open the file from the correct location
        
        // discard entry if not regular file or directory
        if ((currentDirEntry->d_type != DT_REG) && (currentDirEntry->d_type != DT_DIR)) {
            continue;
        }
        
        // skip .DS_Store file
        if (strcmp(currentDirEntry->d_name, ".DS_Store") == 0) continue;
        
        // echo each entry's name if verbatim switch is on
        if (params.isVerbatim()) {
            myOutStream << endl;
            for (int i=0; i<depth; i++) myOutStream << tab;
            myOutStream << "name: " << currentDirEntry->d_name;
            if (currentDirEntry->d_type == DT_DIR) myOutStream << " (dir)";
        }
        
        // recursively handle directories
        if (currentDirEntry->d_type == DT_DIR) {
            travel(path + "/" + currentDirEntry->d_name, currentDirEntry->d_name, depth + 1);
        }
        
        // handle regular files
        if (currentDirEntry->d_type == DT_REG) {
            FileID tempFile = oneFile(currentDirEntry);
            if (tempFile.countFoundWords() > 0) {
                suspiciousFiles.push_back(tempFile);
            }
        }
    }
    
    closedir(dir);
}
