//
// Created by liork on 17/09/17.
//

/**
* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
* Copyright (c) 2016 LIBSCAPI (http://crypto.biu.ac.il/SCAPI)
* This file is part of the SCAPI project.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
* FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* We request that any publication and/or code referring to and/or based on SCAPI contain an appropriate citation to SCAPI, including a reference to
* http://crypto.biu.ac.il/SCAPI.
*
* Libscapi uses several open source libraries. Please see these projects for any further licensing issues.
* For more information , See https://github.com/cryptobiu/libscapi/blob/master/LICENSE.MD
*
* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*/

#include "../../include/infra/Measurement.hpp"
#include "../../include/cryptoInfra/Protocol.hpp"


using namespace std;


Measurement::Measurement(Protocol &protocol)
{
    init(protocol);
}

Measurement::Measurement(Protocol &protocol, vector<string> names)
{
    init(protocol);
    init(names);
}

Measurement::Measurement(string protocolName, int internalIterationsNumber, int partyId, int partiesNumber,
                         string partiesFile)
{
    init(protocolName, internalIterationsNumber, partyId, partiesNumber, partiesFile);
}

Measurement::Measurement(string protocolName, int internalIterationsNumber, int partyId, int partiesNumber,
                         string partiesFile, vector<string> names)
{
    init(protocolName, internalIterationsNumber, partyId, partiesNumber, partiesFile);
    init(names);
}

void Measurement::setTaskNames(vector<string> & names)
{
    init(names);
}

void Measurement::addTaskNames(vector<string> & names)
{
    names.insert(names.end(), m_names.begin(), m_names.end());

    delete m_cpuStartTimes;
    delete m_cpuEndTimes;

    init(names);
}

void Measurement::init(Protocol &protocol)
{
    m_arguments = protocol.getArguments();
    CmdParser parser = protocol.getParser();
    m_protocolName = parser.getValueByKey(m_arguments, "protocolName");
    m_numberOfIterations = stoi(parser.getValueByKey(m_arguments,"internalIterationsNumber"));
    string partyId = parser.getValueByKey(m_arguments, "partyID");

    if(partyId.compare("NotFound") != 0)
        m_partyId =  stoi(partyId);

    m_numOfParties = atoi(parser.getValueByKey(m_arguments, "numParties").c_str());
}

void Measurement::init(string protocolName, int internalIterationsNumber, int partyId, int partiesNumber,
                       string partiesFile)
{
    m_protocolName = protocolName;
    m_numberOfIterations = internalIterationsNumber;
    m_partyId = partyId;
    m_numOfParties = partiesNumber;
}

void Measurement::init(vector <string> names)
{
    m_cpuStartTimes = new vector<vector<double>>(names.size(), vector<double>(m_numberOfIterations,0));
    m_cpuEndTimes = new vector<vector<double>>(names.size(), vector<double>(m_numberOfIterations, 0));
    m_names = move(names);
}



int Measurement::getTaskIdx(string name)
{
    auto it = std::find(m_names.begin(), m_names.end(), name);
    auto idx = distance(m_names.begin(), it);
    return idx;
}



void Measurement::startSubTask(string taskName, int currentIterationNum)
{
    auto now = system_clock::now();
    auto ms = time_point_cast<milliseconds>(now).time_since_epoch().count();
    int taskIdx = getTaskIdx(taskName);
    (*m_cpuStartTimes)[taskIdx][currentIterationNum] = ms;
}

void Measurement::endSubTask(string taskName, int currentIterationNum)
{
    int taskIdx = getTaskIdx(taskName);
    auto now = system_clock::now();
    auto ms = time_point_cast<milliseconds>(now).time_since_epoch().count();
    (*m_cpuEndTimes)[taskIdx][currentIterationNum] = ms - (*m_cpuStartTimes)[taskIdx][currentIterationNum];
}


void Measurement::analyze()
{
    string filePath = getcwdStr();
    string fileName = filePath + "/" + m_protocolName + "*";

    for (int idx = 1; idx< m_arguments.size(); idx++)
    {
        fileName += "*" + m_arguments[idx].second;

    }
    fileName += ".json";

    //party is the root of the json objects
    json party = json::array();

    for (int taskNameIdx = 0; taskNameIdx < m_names.size(); taskNameIdx++)
    {
        //Write for each task name all the iteration
        json task = json::object();
        task["name"] = m_names[taskNameIdx];

        for (int iterationIdx = 0; iterationIdx < m_numberOfIterations; iterationIdx++)
        {
            ostringstream streamObj;
            streamObj << fixed << setprecision(3) << (*m_cpuEndTimes)[taskNameIdx][iterationIdx];
            task["iteration_" + to_string(iterationIdx)] = streamObj.str();
        }

        party.insert(party.begin(), task);
    }

    //send json object to create file
    createJsonFile(party, fileName);
}

void Measurement::createJsonFile(json j, string fileName)
{
    try
    {
        ofstream myfile (fileName, ostream::out);
        myfile << j;
    }

    catch (exception& e)
    {
        cout << "Exception thrown : " << e.what() << endl;
    }
}


Measurement::~Measurement()
{
    analyze();
    delete m_cpuStartTimes;
    delete m_cpuEndTimes;
}

