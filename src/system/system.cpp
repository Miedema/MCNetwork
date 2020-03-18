#include "system.h"
#include "debug.h"


System::System(const std::shared_ptr<ParameterStorage> & parameterStorage) : parameterStorage(parameterStorage) {
    DEBUG_FUNC_START

    acceptorNumber    = parameterStorage->parameters.at("acceptorNumber"   );
    hoppingSiteNumber = parameterStorage->parameters.at("hoppingSiteNumber");
    locLenA           = parameterStorage->parameters.at("a"                );
    storingMode       = parameterStorage->parameters.at("storingMode"      );
    electrodeNumber   = hoppingSiteNumber-acceptorNumber;

    konwnPartRatesSumList.reset(new std::unordered_map<std::vector<bool>,std::shared_ptr<std::vector<double>>>());
    knownRatesSum        .reset(new std::unordered_map<std::vector<bool>,double>());


    DEBUG_FUNC_END
}
System::System(System const & oldSys) : 
                                        parameterStorage          (oldSys.parameterStorage         ),
                                        acceptorNumber            (oldSys.acceptorNumber           ),
                                        hoppingSiteNumber         (oldSys.hoppingSiteNumber        ),
                                        electrodeNumber           (oldSys.electrodeNumber          ),
                                        donorPositionsX           (oldSys.donorPositionsX          ),
                                        donorPositionsY           (oldSys.donorPositionsY          ),
                                        acceptorPositionsX        (oldSys.acceptorPositionsX       ),
                                        acceptorPositionsY        (oldSys.acceptorPositionsY       ),
                                        electrodePositionsX       (oldSys.electrodePositionsX      ),
                                        electrodePositionsY       (oldSys.electrodePositionsY      ),
                                        distances                 (oldSys.distances                ),
                                        pairEnergies              (oldSys.pairEnergies             ),
                                        occupation                (oldSys.occupation               ),
                                        locLenA                   (oldSys.locLenA                  ),
                                        constantRatesSumPart      (oldSys.constantRatesSumPart     ),
                                        storingMode               (oldSys.storingMode              ),
                                        interactionPartners       (oldSys.interactionPartners      ),
                                        hoppingPartnersAcceptors  (oldSys.hoppingPartnersAcceptors ),
                                        hoppingPartnersElectrodes (oldSys.hoppingPartnersElectrodes),
                                        knownRatesSum             (oldSys.knownRatesSum            ) {
    DEBUG_FUNC_START

    if(not oldSys.readyForRun){
        throw std::invalid_argument("copying system that isnt ready for run is forbidden!");
    }

    energies       = new double[hoppingSiteNumber];
    currentCounter = new int   [hoppingSiteNumber];
    outputCurrentCounter = & currentCounter[int(parameterStorage->parameters.at("outputElectrode")+parameterStorage->parameters["acceptorNumber"])];  
    for(int i=0;i<hoppingSiteNumber;i++){
        energies      [i] = oldSys.energies      [i];
        currentCounter[i] = oldSys.currentCounter[i];
    }

    deltaEnergies = new double[hoppingSiteNumber*hoppingSiteNumber];
    rates         = new double[hoppingSiteNumber*hoppingSiteNumber];
    baseRates     = new double[hoppingSiteNumber*hoppingSiteNumber];
    for(int i=0;i<hoppingSiteNumber*hoppingSiteNumber;i++){
        deltaEnergies[i] = oldSys.deltaEnergies[i];
        rates        [i] = oldSys.rates        [i];
        baseRates    [i] = oldSys.baseRates    [i];
    }

    konwnPartRatesSumList.reset(new std::unordered_map<std::vector<bool>,std::shared_ptr<std::vector<double>>>());
    knownRatesSum        .reset(new std::unordered_map<std::vector<bool>,double>());

    //setup finEle
    if (parameterStorage->geometry == "rect"){
        finEle = std::make_unique<FiniteElementeRect>(parameterStorage->parameters.at("lenX"),parameterStorage->parameters.at("lenY"),parameterStorage->parameters.at("finiteElementsResolution"));
        //set electrodes
        for(int i=0; i< electrodeNumber; i++){
            switch (parameterStorage->electrodes[i].edge){
                case 0:
                case 1:
                    finEle->setElectrode(0, parameterStorage->parameters.at("lenY")*parameterStorage->electrodes[i].pos-0.5*parameterStorage->parameters.at("electrodeWidth"),parameterStorage->parameters.at("lenY")*parameterStorage->electrodes[i].pos+0.5*parameterStorage->parameters.at("electrodeWidth"),parameterStorage->electrodes[i].edge);
                    break;
                case 2:
                case 3:
                    finEle->setElectrode(0, parameterStorage->parameters.at("lenX")*parameterStorage->electrodes[i].pos-0.5*parameterStorage->parameters.at("electrodeWidth"),parameterStorage->parameters.at("lenX")*parameterStorage->electrodes[i].pos+0.5*parameterStorage->parameters.at("electrodeWidth"),parameterStorage->electrodes[i].edge);
                    break;
            }
        }
    }
    else if (parameterStorage->geometry == "circle"){
        finEle = std::make_unique<FiniteElementeCircle>(parameterStorage->parameters.at("radius"),parameterStorage->parameters.at("finiteElementsResolution"));
        //set electrodes
        for(int i=0; i< electrodeNumber; i++){
            finEle->setElectrode(0, parameterStorage->electrodes[i].pos/360*2*PI-0.5*parameterStorage->parameters.at("electrodeWidth")/parameterStorage->parameters.at("radius"),  parameterStorage->electrodes[i].pos/360*2*PI+0.5*parameterStorage->parameters.at("electrodeWidth")/parameterStorage->parameters.at("radius"));
        }
    }
    finEle->initRun();

    *(finEle->solutionVector) = oldSys.getPotential();

    readyForRun=true;
    DEBUG_FUNC_END
}

void System::initilizeMatrices(){
    DEBUG_FUNC_START
    //setup matrices
    pairEnergies   = new double[acceptorNumber*acceptorNumber];
    distances      = new double[hoppingSiteNumber*hoppingSiteNumber];
    deltaEnergies  = new double[hoppingSiteNumber*hoppingSiteNumber];
    rates          = new double[hoppingSiteNumber*hoppingSiteNumber];
    baseRates      = new double[hoppingSiteNumber*hoppingSiteNumber];
    energies       = new double[hoppingSiteNumber];
    currentCounter = new int   [hoppingSiteNumber];
    outputCurrentCounter = & currentCounter[int(parameterStorage->parameters.at("outputElectrode")+parameterStorage->parameters["acceptorNumber"])];  


    donorPositionsX     = new double[int(parameterStorage->parameters.at("donorNumber"))];
    donorPositionsY     = new double[int(parameterStorage->parameters.at("donorNumber"))];
    acceptorPositionsX  = new double[acceptorNumber ];
    acceptorPositionsY  = new double[acceptorNumber ];
    electrodePositionsX = new double[electrodeNumber];
    electrodePositionsY = new double[electrodeNumber];

    DEBUG_FUNC_END
}

void System::createRandomNewDevice(){
    DEBUG_FUNC_START


    if (parameterStorage->geometry == "rect"){
        // need to set electrodes first, to hold minDist for acceptors. (redundance to getReadyForRun)
        //set electrodes
        for(int i=0; i< electrodeNumber; i++){
            switch (parameterStorage->electrodes[i].edge){
                case 0:
                    electrodePositionsX[i]=0;
                    electrodePositionsY[i]=parameterStorage->parameters.at("lenY")*parameterStorage->electrodes[i].pos;
                    break;
                case 1:
                    electrodePositionsX[i]=parameterStorage->parameters.at("lenX");
                    electrodePositionsY[i]=parameterStorage->parameters.at("lenY")*parameterStorage->electrodes[i].pos;
                    break;
                case 2:
                    electrodePositionsX[i]=parameterStorage->parameters.at("lenX")*parameterStorage->electrodes[i].pos;
                    electrodePositionsY[i]=0;
                    break;
                case 3:
                    electrodePositionsX[i]=parameterStorage->parameters.at("lenX")*parameterStorage->electrodes[i].pos;
                    electrodePositionsY[i]=parameterStorage->parameters.at("lenY");
                    break;
            }
        }



        // create acceptors, setup position (first part of hoppingSites, rest are electrodes)
        // check minDist to other acceptors and electrodes
        int iteration=0;
        for(int i=0;i<acceptorNumber;i++){
            iteration=0;

            rollAgain:
            iteration++;
            if (iteration>10000){
                throw std::logic_error("could not find device, reduce minDist or acceptorNumber");
            }
            acceptorPositionsX[i]=parameterStorage->parameters.at("lenX")*enhance::random_double(0,1);
            acceptorPositionsY[i]=parameterStorage->parameters.at("lenY")*enhance::random_double(0,1);
            //check acc-acc dist
            for(int j=0;j<i;j++){
                if(std::sqrt(std::pow(acceptorPositionsX[i]-acceptorPositionsX[j],2)+std::pow(acceptorPositionsY[i]-acceptorPositionsY[j],2)) < parameterStorage->parameters.at("minDist")){
                    goto rollAgain;
                }
            }
            //check el-acc dist
            for(int j=0;j<electrodeNumber;j++){
                if(std::sqrt(std::pow(acceptorPositionsX[i]-electrodePositionsX[j],2)+std::pow(acceptorPositionsY[i]-electrodePositionsY[j],2)) < parameterStorage->parameters.at("minDist")){
                    goto rollAgain;
                }
            }
        }

        // set donor positions
        for(int i=0;i<parameterStorage->parameters.at("donorNumber");i++){
            donorPositionsX[i]=parameterStorage->parameters.at("lenX")*enhance::random_double(0,1);
            donorPositionsY[i]=parameterStorage->parameters.at("lenY")*enhance::random_double(0,1);
        }
    }



    else if (parameterStorage->geometry == "circle"){
        // need to set electrodes first, to hold minDist for acceptors. (redundance to getReadyForRun)
        //set electrodes
        for(int i=0; i< electrodeNumber; i++){
            electrodePositionsX[i]=parameterStorage->parameters.at("radius")*cos(parameterStorage->electrodes[i].pos/360*2*PI);
            electrodePositionsY[i]=parameterStorage->parameters.at("radius")*sin(parameterStorage->electrodes[i].pos/360*2*PI);
        }


        // create acceptors, setup position (first part of hoppingSites, rest are electrodes)
        // check minDist to other acceptors and electrodes
        int iteration=0;
        for(int i=0;i<acceptorNumber;i++){
            iteration=0;

            rollAgainCirc:
            iteration++;
            if (iteration>10000){
                throw std::logic_error("could not find device, reduce minDist or acceptorNumber");
            }
            acceptorPositionsX[i]=2*parameterStorage->parameters.at("radius")*enhance::random_double(0,1)-parameterStorage->parameters.at("radius");
            acceptorPositionsY[i]=2*parameterStorage->parameters.at("radius")*enhance::random_double(0,1)-parameterStorage->parameters.at("radius");
            //check in circle
            if( (acceptorPositionsX[i]*acceptorPositionsX[i]+acceptorPositionsY[i]*acceptorPositionsY[i]) > parameterStorage->parameters.at("radius")*parameterStorage->parameters.at("radius")){
                goto rollAgainCirc;
            }
            //check acc-acc dist
            for(int j=0;j<i;j++){
                if((std::pow(acceptorPositionsX[i]-acceptorPositionsX[j],2)+std::pow(acceptorPositionsY[i]-acceptorPositionsY[j],2)) < parameterStorage->parameters.at("minDist")*parameterStorage->parameters.at("minDist")){
                    goto rollAgainCirc;
                }
            }
            //check el-acc dist
            for(int j=0;j<electrodeNumber;j++){
                if((std::pow(acceptorPositionsX[i]-electrodePositionsX[j],2)+std::pow(acceptorPositionsY[i]-electrodePositionsY[j],2)) < parameterStorage->parameters.at("minDist")*parameterStorage->parameters.at("minDist")){
                    goto rollAgainCirc;
                }
            }
        }

        // set donor positions
        for(int i=0;i<parameterStorage->parameters.at("donorNumber");i++){
            rollAgainCirc2:
            donorPositionsX[i]=2*parameterStorage->parameters.at("radius")*enhance::random_double(0,1)-parameterStorage->parameters.at("radius");
            donorPositionsY[i]=2*parameterStorage->parameters.at("radius")*enhance::random_double(0,1)-parameterStorage->parameters.at("radius");
            //check in circle
            if( (donorPositionsX[i]*donorPositionsX[i]+donorPositionsY[i]*donorPositionsY[i]) > parameterStorage->parameters.at("radius")*parameterStorage->parameters.at("radius")){
                goto rollAgainCirc2;
            }
        }
    }




    //save device
    std::string deviceFileName=parameterStorage->workingDirecotry + "device.txt";
    std::ofstream deviceFile;
    deviceFile.open (deviceFileName,std::ios::trunc);
    deviceFile<<parameterStorage->geometry<<std::endl;
    deviceFile<<"acceptors: posX, posY"<<std::endl;
    for(int i=0;i<acceptorNumber;i++){
        deviceFile<<acceptorPositionsX[i]*parameterStorage->parameters["R"]<<" "<<acceptorPositionsY[i]*parameterStorage->parameters["R"]<<std::endl;
    }
    deviceFile<<std::endl;


    deviceFile<<"donors: posX, posY"<<std::endl;
    for(int i=0;i<parameterStorage->parameters.at("donorNumber");i++){
        deviceFile<<donorPositionsX[i]*parameterStorage->parameters["R"]<<" "<<donorPositionsY[i]*parameterStorage->parameters["R"]<<std::endl;
    }

    deviceFile.close();

    DEBUG_FUNC_END
}

void System::loadDevice(){
    DEBUG_FUNC_START

    std::string deviceFileName=parameterStorage->workingDirecotry + "device.txt";
    std::ifstream deviceFile(deviceFileName);
    std::string line;
    double posXBuffer, posYBuffer;


    //check correct geometry
    std::getline(deviceFile, line);
    if( line != parameterStorage->geometry) throw std::invalid_argument( "geometries in device file and input file do not match");

    //trash second line
    std::getline(deviceFile, line);

    //load acceptors
    for(int i=0;i<acceptorNumber;i++){
        std::getline(deviceFile, line);
        std::istringstream iss(line);
        if(!(iss>>posXBuffer>>posYBuffer)){
            if (line == "") throw std::invalid_argument( "incorrect number of acceptors in device file");
            else            throw std::invalid_argument( "device file corrupted! cant read acceptor in line: " + line);
        }
        acceptorPositionsX[i]=posXBuffer/parameterStorage->parameters["R"];
        acceptorPositionsY[i]=posYBuffer/parameterStorage->parameters["R"];
    }

    //trash 1 line
    std::getline(deviceFile, line);

    //check correct file length
    std::getline(deviceFile, line);
    std::string stringBuffer;
    std::istringstream iss(line);
    iss>>stringBuffer;
    if(stringBuffer != "donors:") throw std::invalid_argument( "incorrect number of acceptors in device file");


    // set donor positions
    for(int i=0;i<parameterStorage->parameters.at("donorNumber");i++){
        std::getline(deviceFile, line);
        std::istringstream iss(line);
        if(!(iss>>posXBuffer>>posYBuffer)) throw std::invalid_argument( "device file corrupted! cant read donor in line: " + line);
        donorPositionsX[i]=posXBuffer/parameterStorage->parameters["R"];
        donorPositionsY[i]=posYBuffer/parameterStorage->parameters["R"];
    }
    
    deviceFile.close();

    DEBUG_FUNC_END
}

void System::getReadyForRun(){
    DEBUG_FUNC_START

    //set electrodes
    if (parameterStorage->geometry == "rect"){
        for(int i=0; i< electrodeNumber; i++){
            switch (parameterStorage->electrodes[i].edge){
                case 0:
                    electrodePositionsX[i]=0;
                    electrodePositionsY[i]=parameterStorage->parameters.at("lenY")*parameterStorage->electrodes[i].pos;
                    break;
                case 1:
                    electrodePositionsX[i]=parameterStorage->parameters.at("lenX");
                    electrodePositionsY[i]=parameterStorage->parameters.at("lenY")*parameterStorage->electrodes[i].pos;
                    break;
                case 2:
                    electrodePositionsX[i]=parameterStorage->parameters.at("lenX")*parameterStorage->electrodes[i].pos;
                    electrodePositionsY[i]=0;
                    break;
                case 3:
                    electrodePositionsX[i]=parameterStorage->parameters.at("lenX")*parameterStorage->electrodes[i].pos;
                    electrodePositionsY[i]=parameterStorage->parameters.at("lenY");
                    break;
            }
        }
    }
    else if (parameterStorage->geometry == "circle"){
        for(int i=0; i< electrodeNumber; i++){
            electrodePositionsX[i]=parameterStorage->parameters.at("radius")*cos(parameterStorage->electrodes[i].pos/360*2*PI);
            electrodePositionsY[i]=parameterStorage->parameters.at("radius")*sin(parameterStorage->electrodes[i].pos/360*2*PI);
        }
    }





    //init laplace solver
    if (parameterStorage->geometry == "rect"){
        finEle = std::make_unique<FiniteElementeRect>(parameterStorage->parameters.at("lenX"),parameterStorage->parameters.at("lenY"),parameterStorage->parameters.at("finiteElementsResolution"));
        //set electrodes
        for(int i=0; i< electrodeNumber; i++){
            switch (parameterStorage->electrodes[i].edge){
                case 0:
                case 1:
                    finEle->setElectrode(0, parameterStorage->parameters.at("lenY")*parameterStorage->electrodes[i].pos-0.5*parameterStorage->parameters.at("electrodeWidth"),parameterStorage->parameters.at("lenY")*parameterStorage->electrodes[i].pos+0.5*parameterStorage->parameters.at("electrodeWidth"),parameterStorage->electrodes[i].edge);
                    break;
                case 2:
                case 3:
                    finEle->setElectrode(0, parameterStorage->parameters.at("lenX")*parameterStorage->electrodes[i].pos-0.5*parameterStorage->parameters.at("electrodeWidth"),parameterStorage->parameters.at("lenX")*parameterStorage->electrodes[i].pos+0.5*parameterStorage->parameters.at("electrodeWidth"),parameterStorage->electrodes[i].edge);
                    break;
            }
        }
    }
    else if (parameterStorage->geometry == "circle"){
        finEle = std::make_unique<FiniteElementeCircle>(parameterStorage->parameters.at("radius"),parameterStorage->parameters.at("finiteElementsResolution"));
        //set electrodes
        for(int i=0; i< electrodeNumber; i++){
            finEle->setElectrode(0, parameterStorage->electrodes[i].pos/360*2*PI-0.5*parameterStorage->parameters.at("electrodeWidth")/parameterStorage->parameters.at("radius"),  parameterStorage->electrodes[i].pos/360*2*PI+0.5*parameterStorage->parameters.at("electrodeWidth")/parameterStorage->parameters.at("radius"));
        }
    }
    finEle->initRun(true);


    // calc distances and pair energies
    // acc<->acc
    double minDist = 0.0;
    for(int i=0;i<acceptorNumber;i++){
        for(int j=0;j<i;j++){
            distances[i*hoppingSiteNumber+j]=std::sqrt(std::pow(acceptorPositionsX[i]-acceptorPositionsX[j],2)+std::pow(acceptorPositionsY[i]-acceptorPositionsY[j],2));
            distances[j*hoppingSiteNumber+i]=std::sqrt(std::pow(acceptorPositionsX[i]-acceptorPositionsX[j],2)+std::pow(acceptorPositionsY[i]-acceptorPositionsY[j],2));
            // std::cout<<distances[i*hoppingSiteNumber+j]<<std::endl;
            pairEnergies[i*acceptorNumber+j]=-parameterStorage->parameters.at("I0")/distances[i*hoppingSiteNumber+j];
            pairEnergies[j*acceptorNumber+i]=-parameterStorage->parameters.at("I0")/distances[j*hoppingSiteNumber+i];
            // std::cout<<"pair e "<<pairEnergies[i*acceptorNumber+j]<<std::endl;
        }
        pairEnergies[i*(acceptorNumber+1)] = 0; //[i*(hoppingSiteNumber+1)] = [i,i]
        distances   [i*(hoppingSiteNumber+1)] = 0;
    }
    // acc->el
    for(int i=0;i<acceptorNumber;i++){
        for(int j=0;j<electrodeNumber;j++){
            distances[i*hoppingSiteNumber+(j+acceptorNumber)]=std::sqrt(std::pow(acceptorPositionsX[i]-electrodePositionsX[j],2)+std::pow(acceptorPositionsY[i]-electrodePositionsY[j],2));
        }
    }
    // el->acc
    for(int i=0;i<electrodeNumber;i++){
        for(int j=0;j<acceptorNumber;j++){
            distances[(i+acceptorNumber)*hoppingSiteNumber+j]=std::sqrt(std::pow(electrodePositionsX[i]-acceptorPositionsX[j],2)+std::pow(electrodePositionsY[i]-acceptorPositionsY[j],2));
        }
    }
    // el<->el
    for(int i=0;i<electrodeNumber;i++){
        for(int j=0;j<i;j++){
            distances[(i+acceptorNumber)*hoppingSiteNumber+(j+acceptorNumber)]=std::sqrt(std::pow(electrodePositionsX[i]-electrodePositionsX[j],2)+std::pow(electrodePositionsY[i]-electrodePositionsY[j],2));
            distances[(j+acceptorNumber)*hoppingSiteNumber+(i+acceptorNumber)]=std::sqrt(std::pow(electrodePositionsX[i]-electrodePositionsX[j],2)+std::pow(electrodePositionsY[i]-electrodePositionsY[j],2));
        }
        distances[(i+acceptorNumber)*(hoppingSiteNumber+1)]=0;
    }


    // set hoppingPartners and use same loop to calc baseRates
    int lowDistblocked = 0;
    for(int i=0;i<hoppingSiteNumber;i++){
        hoppingPartnersAcceptors.push_back(std::vector<int>());
        for(int j=0;j<acceptorNumber;j++){
            if (i != j and distances[i*hoppingSiteNumber+j] < parameterStorage->parameters.at("maxHoppingDist") and distances[i*hoppingSiteNumber+j] >  parameterStorage->parameters.at("minHoppingDist")){
                hoppingPartnersAcceptors[i].push_back(j);
                baseRates[i*hoppingSiteNumber+j] = enhance::mediumFastExp(-2*distances[i*hoppingSiteNumber+j]/locLenA);
                // std::cout<<"i "<<i<<" j: "<<j<<" rate: "<<baseRates[i*hoppingSiteNumber+j]<<std::endl;
            }
            //only for printing
            else if (i != j and distances[i*hoppingSiteNumber+j] < parameterStorage->parameters.at("minHoppingDist")){
                lowDistblocked++;
            }
        }
        hoppingPartnersElectrodes.push_back(std::vector<int>());
        for(int j=acceptorNumber;j<hoppingSiteNumber;j++){
            if (i != j and distances[i*hoppingSiteNumber+j] < parameterStorage->parameters.at("maxHoppingDist") and distances[i*hoppingSiteNumber+j] >  parameterStorage->parameters.at("minHoppingDist")){
                hoppingPartnersElectrodes[i].push_back(j);
                baseRates[i*hoppingSiteNumber+j] = enhance::mediumFastExp(-2*distances[i*hoppingSiteNumber+j]/locLenA);
                // std::cout<<"i "<<i<<" j: "<<j<<" rate: "<<baseRates[i*hoppingSiteNumber+j]<<std::endl;
            }
            //only for printing
            else if (i != j and distances[i*hoppingSiteNumber+j] < parameterStorage->parameters.at("minHoppingDist")){
                lowDistblocked++;
            }
        }
    }
    if (lowDistblocked > 0){
        std::cout<<"hopping connections blocked due to small distance: "<<lowDistblocked/2<<std::endl;
    }

    // set interaction partners
    for(int i=0;i<acceptorNumber;i++){
        interactionPartners.push_back(std::vector<int>());
        for(int j=0;j<acceptorNumber;j++){
            if (i != j and distances[i*hoppingSiteNumber+j] < parameterStorage->parameters.at("maxInteractionDist") ){
                interactionPartners[i].push_back(j);
            }
        }
    }


    // set start occupation of acceptors
    std::vector<bool> occupationBuffer(acceptorNumber);
    std::vector<int> indicesUnoccupied {};

    for(int i=0;i<acceptorNumber;i++){
        indicesUnoccupied.push_back(i);
        occupationBuffer[i]=false;
    }
    for(int i=0;i<acceptorNumber-parameterStorage->parameters.at("donorNumber");i++){
        int index=enhance::random_int(0,acceptorNumber-i-1);
        occupationBuffer[indicesUnoccupied[index]]=true;
        indicesUnoccupied.erase(indicesUnoccupied.begin()+index);
    }

    setOccupation(occupationBuffer);

    //set electrode energy
    for(int i=0;i<electrodeNumber;i++){
        energies[i+acceptorNumber]=0;
    }

    readyForRun=true;

    DEBUG_FUNC_END
}



void System::setOccupation(std::vector<bool> const & newOccupation){
    DEBUG_FUNC_START
    
    occupation = newOccupation;

    // set acceptor energies
    // donor interaction
    for(int i=0;i<acceptorNumber;i++){
        energies[i]=finEle->getPotential(acceptorPositionsX[i],acceptorPositionsY[i])*parameterStorage->parameters.at("e")/parameterStorage->parameters.at("kT");
        for(int j=0;j<parameterStorage->parameters.at("donorNumber");j++){
            energies[i]+=parameterStorage->parameters.at("I0")*1/std::sqrt(std::pow(acceptorPositionsX[i]-donorPositionsX[j],2)+std::pow(acceptorPositionsY[i]-donorPositionsY[j],2));
        }
    }
    //set coulomb part (with start occupation)
    for(int i=0;i<acceptorNumber;i++){ //coulomb interaction only with acceptors and..
        if (not occupation[i]){ //.. only if they are unoccupied
            for(int const & j : interactionPartners[i]){ //self interaction is ignored, bc pairEnergies[i][i]=0
                energies[j]+=pairEnergies[i*acceptorNumber+j];
            }
        }
    }

    DEBUG_FUNC_END
}

std::vector<bool> const & System::getOccupation(){
    DEBUG_FUNC_START
    DEBUG_FUNC_END
    return occupation;
}


void System::updateRatesStoringMode(){
    DEBUG_FUNC_START

    if (knownRatesSum->count(occupation)){ //state known
        ratesSum         = knownRatesSum->at(occupation);
        partRatesSumList = konwnPartRatesSumList->at(occupation);
        ratesInMemory=true; //tell findSwap to use binary search

        // std::cout<<"found state: "<<occupation<<std::endl;
    }
    else{ //state unknown
        if (storeKnownStates){ //still saving (memory limit not exceeded)
            
            //following part is nearly calcRates(), only ratesSum is not calculated (set as last component of partRatesSumList afterwards)
            {
                //acc acc hopp
                for(int i=0;i<acceptorNumber;i++){
                    if (occupation[i]){
                        for(int const & j : hoppingPartnersAcceptors[i]){
                            if (not occupation[j]){
                                // std::cout<<" ----- "<<i<<" "<<j<<"delta E "<<deltaEnergies[i*hoppingSiteNumber+j]<<std::endl;
                                deltaEnergies[i*hoppingSiteNumber+j]=energies[j]-energies[i]+pairEnergies[i*acceptorNumber+j];
                                // std::cout<<" ei "<<energies[i]<<" ej "<<energies[j]<<" epair "<<pairEnergies[i*acceptorNumber+j]<<std::endl;
                                if (deltaEnergies[i*hoppingSiteNumber+j] <= 0){
                                    rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j];
                                }
                                else{
                                    rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j]*enhance::mediumFastExp(-deltaEnergies[i*hoppingSiteNumber+j]);
                                }
                            }
                            else{
                                rates[i*hoppingSiteNumber+j]=0;
                                // deltaEnergies[i*hoppingSiteNumber+j]=0;
                            }
                        }
                    }
                    else{
                        for(int const & j : hoppingPartnersAcceptors[i]){
                            rates[i*hoppingSiteNumber+j]=0;
                            // deltaEnergies[i*hoppingSiteNumber+j]=0;
                        }
                    }
                }

                // el-acc hopp
                for(int i=acceptorNumber;i<hoppingSiteNumber;i++){
                    for(int const & j : hoppingPartnersAcceptors[i]){
                        if (not occupation[j]){
                            deltaEnergies[i*hoppingSiteNumber+j]=energies[j]-energies[i];
                            if (deltaEnergies[i*hoppingSiteNumber+j] <= 0){
                                rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j];
                            }
                            else{
                                rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j]*enhance::mediumFastExp(-deltaEnergies[i*hoppingSiteNumber+j]);
                            }
                        }
                        else{
                            rates[i*hoppingSiteNumber+j]=0;
                            // deltaEnergies[i*hoppingSiteNumber+j]=0;
                        }
                    }
                }

                //acc-el hopp
                for(int i=0;i<acceptorNumber;i++){
                    if (occupation[i]){
                        for(int const & j : hoppingPartnersElectrodes[i]){
                            deltaEnergies[i*hoppingSiteNumber+j]=energies[j]-energies[i];
                            if (deltaEnergies[i*hoppingSiteNumber+j] <= 0){
                                rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j];
                            }
                            else{
                                rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j]*enhance::mediumFastExp(-deltaEnergies[i*hoppingSiteNumber+j]);
                            }
                        }
                    }
                    else{
                        for(int const & j : hoppingPartnersElectrodes[i]){
                            rates[i*hoppingSiteNumber+j]=0;
                            // deltaEnergies[i*hoppingSiteNumber+j]=0;
                        }
                    }
                }
            }

            partRatesSumList = std::make_shared<std::vector<double>>(hoppingSiteNumber*hoppingSiteNumber+1);
            (*partRatesSumList)[0]=0;
            for(int i=0; i < hoppingSiteNumber*hoppingSiteNumber; i++){
                (*partRatesSumList)[i+1]=(*partRatesSumList)[i]+rates[i];
            }
            ratesSum=(*partRatesSumList)[hoppingSiteNumber*hoppingSiteNumber];

            knownRatesSum->emplace(occupation,ratesSum);
            konwnPartRatesSumList->emplace(occupation,partRatesSumList);
            ratesInMemory=true; //tell findSwap to use binary search

        }
        else{ //memory limit reached
            ratesInMemory=false; //tell findSwap to use comparison search
            updateRates();
        }
    }


    DEBUG_FUNC_END
}

void System::updateRates(){
    ratesSum = constantRatesSumPart;

    //acc acc hopp
    for(int i=0;i<acceptorNumber;i++){
        if (occupation[i]){
            for(int const & j : hoppingPartnersAcceptors[i]){
                if (not occupation[j]){
                    // std::cout<<" ----- "<<i<<" "<<j<<" delta E "<<deltaEnergies[i*hoppingSiteNumber+j]<<std::endl;
                    deltaEnergies[i*hoppingSiteNumber+j]=energies[j]-energies[i]+pairEnergies[i*acceptorNumber+j];
                    // std::cout<<" ei "<<energies[i]<<" ej "<<energies[j]<<" epair "<<pairEnergies[i*acceptorNumber+j]<<std::endl;
                    if (deltaEnergies[i*hoppingSiteNumber+j] <= 0){
                        rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j];
                    }
                    else{
                        rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j]*enhance::mediumFastExp(-deltaEnergies[i*hoppingSiteNumber+j]);
                    }
                    ratesSum+=rates[i*hoppingSiteNumber+j];
                }
            }
        }
    }

    // el-acc hopp
    for(int i=acceptorNumber;i<hoppingSiteNumber;i++){
        for(int const & j : hoppingPartnersAcceptors[i]){
            if (not occupation[j]){
                deltaEnergies[i*hoppingSiteNumber+j]=energies[j]-energies[i];
                if (deltaEnergies[i*hoppingSiteNumber+j] <= 0){
                    rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j];
                }
                else{
                    rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j]*enhance::mediumFastExp(-deltaEnergies[i*hoppingSiteNumber+j]);
                }
                ratesSum+=rates[i*hoppingSiteNumber+j];
            }
        }
    }


    //acc-el hopp
    for(int i=0;i<acceptorNumber;i++){
        if (occupation[i]){
            for(int const & j : hoppingPartnersElectrodes[i]){
                deltaEnergies[i*hoppingSiteNumber+j]=energies[j]-energies[i];
                if (deltaEnergies[i*hoppingSiteNumber+j] <= 0){
                    rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j];
                }
                else{
                    rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j]*enhance::mediumFastExp(-deltaEnergies[i*hoppingSiteNumber+j]);
                }
                ratesSum+=rates[i*hoppingSiteNumber+j];
            }
        }
    }
}


void System::updatePotential(std::vector<double> const & voltages){
    DEBUG_FUNC_START

    resetPotential();

    //recalc potential
    for(int i=0;i < electrodeNumber;i++){
        finEle->updateElectrodeVoltage(i,voltages[i]);
    }
    finEle->run();

    setNewPotential();

    #ifdef SWAPTRACKER
        swapTrackFile.close(); // swapTracker
        swapTrackFile.open(std::string("swapTrackFile")+std::to_string(fileNumber)+std::string(".txt"), std::ios::out); // swapTracker
        fileNumber++; // swapTracker
    #endif

    DEBUG_FUNC_END
}

void System::updatePotential(mfem::GridFunction const & potential){
    DEBUG_FUNC_START

    resetPotential();

    //set new potential
    *(finEle->solutionVector) = potential;


    setNewPotential();

    #ifdef SWAPTRACKER
        swapTrackFile.close(); // swapTracker
        swapTrackFile.open(std::string("swapTrackFile")+std::to_string(fileNumber)+std::string(".txt"), std::ios::out); // swapTracker
        fileNumber++; // swapTracker
    #endif

    DEBUG_FUNC_END
}

void System::updateOccupationAndPotential(std::vector<bool> const & newOccupation, mfem::GridFunction  const & potential){
    DEBUG_FUNC_START
    
    //set new potential
    *(finEle->solutionVector) = potential;

    // set acceptor energies
    // donor interaction
    for(int i=0;i<acceptorNumber;i++){
        energies[i]=finEle->getPotential(acceptorPositionsX[i],acceptorPositionsY[i])*parameterStorage->parameters.at("e")/parameterStorage->parameters.at("kT");
        for(int j=0;j<parameterStorage->parameters.at("donorNumber");j++){
            energies[i]+=parameterStorage->parameters.at("I0")*1/std::sqrt(std::pow(acceptorPositionsX[i]-donorPositionsX[j],2)+std::pow(acceptorPositionsY[i]-donorPositionsY[j],2));
        }
    }
    //set coulomb part (with start occupation)
    for(int i=0;i<acceptorNumber;i++){ //coulomb interaction only with acceptors and..
        if (not occupation[i]){ //.. only if they are unoccupied
            for(int const & j : interactionPartners[i]){ //self interaction is ignored, bc pairEnergies[i][i]=0
                energies[j]+=pairEnergies[i*acceptorNumber+j];
            }
        }
    }

    //set new electrode energies
    for(int i=0;i<electrodeNumber;i++){
        energies[(i+acceptorNumber)]=finEle->getPotential(electrodePositionsX[i],electrodePositionsY[i])*parameterStorage->parameters.at("e")/parameterStorage->parameters.at("kT");
    }

    //set deltaEnergies and rates (only constant part = el-el interaction)
    //init all to zero
    for(int i=acceptorNumber;i<hoppingSiteNumber;i++){
        for(int j=acceptorNumber;j<i;j++){
            deltaEnergies[i*hoppingSiteNumber+j]=0;
            deltaEnergies[j*hoppingSiteNumber+i]=0;
            rates[i*hoppingSiteNumber+j]=0;
            rates[j*hoppingSiteNumber+i]=0;
        }
    }
    constantRatesSumPart = 0;
    for(int i=acceptorNumber;i<hoppingSiteNumber;i++){
        for(int const & j : hoppingPartnersElectrodes[i]){
            deltaEnergies[i*hoppingSiteNumber+j]=energies[j]-energies[i];
            deltaEnergies[j*hoppingSiteNumber+i]=energies[i]-energies[j];

            if (deltaEnergies[i*hoppingSiteNumber+j] < 0){
                rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j];
                rates[j*hoppingSiteNumber+i]=baseRates[j*hoppingSiteNumber+i]*enhance::mediumFastExp(-deltaEnergies[j*hoppingSiteNumber+i]);
            }
            else if (deltaEnergies[i*hoppingSiteNumber+j] > 0){
                rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j]*enhance::mediumFastExp(-deltaEnergies[i*hoppingSiteNumber+j]);
                rates[j*hoppingSiteNumber+i]=baseRates[j*hoppingSiteNumber+i];
            }
            else{
                rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j];
                rates[j*hoppingSiteNumber+i]=baseRates[j*hoppingSiteNumber+i];
            }
            constantRatesSumPart+=rates[i*hoppingSiteNumber+j];
            constantRatesSumPart+=rates[j*hoppingSiteNumber+i];
        }
    }

    #ifdef SWAPTRACKER
        swapTrackFile.close(); // swapTracker
        swapTrackFile.open(std::string("swapTrackFile")+std::to_string(fileNumber)+std::string(".txt"), std::ios::out); // swapTracker
        fileNumber++; // swapTracker
    #endif

    DEBUG_FUNC_END
}

void System::resetPotential(){
    DEBUG_FUNC_START
    
    //reset old potential
    for(int i=0;i<acceptorNumber;i++){
        energies[i]-=finEle->getPotential(acceptorPositionsX[i],acceptorPositionsY[i])*parameterStorage->parameters.at("e")/parameterStorage->parameters.at("kT");
    }
    for(int i=0;i<electrodeNumber;i++){
        energies[(i+acceptorNumber)]-=finEle->getPotential(electrodePositionsX[i],electrodePositionsY[i])*parameterStorage->parameters.at("e")/parameterStorage->parameters.at("kT");
    }

    DEBUG_FUNC_END
}


void System::setNewPotential(){
    DEBUG_FUNC_START

    //set new potential
    for(int i=0;i<acceptorNumber;i++){
        energies[i]+=finEle->getPotential(acceptorPositionsX[i],acceptorPositionsY[i])*parameterStorage->parameters.at("e")/parameterStorage->parameters.at("kT");
    }
    for(int i=0;i<electrodeNumber;i++){
        energies[(i+acceptorNumber)]+=finEle->getPotential(electrodePositionsX[i],electrodePositionsY[i])*parameterStorage->parameters.at("e")/parameterStorage->parameters.at("kT");
    }


    //set deltaEnergies and rates (only constant part = el-el interaction)
    //init all to zero
    for(int i=acceptorNumber;i<hoppingSiteNumber;i++){
        for(int j=acceptorNumber;j<i;j++){
            deltaEnergies[i*hoppingSiteNumber+j]=0;
            deltaEnergies[j*hoppingSiteNumber+i]=0;
            rates[i*hoppingSiteNumber+j]=0;
            rates[j*hoppingSiteNumber+i]=0;
        }
    }
    constantRatesSumPart = 0;
    for(int i=acceptorNumber;i<hoppingSiteNumber;i++){
        for(int const & j : hoppingPartnersElectrodes[i]){
            deltaEnergies[i*hoppingSiteNumber+j]=energies[j]-energies[i];
            deltaEnergies[j*hoppingSiteNumber+i]=energies[i]-energies[j];

            if (deltaEnergies[i*hoppingSiteNumber+j] < 0){
                rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j];
                rates[j*hoppingSiteNumber+i]=baseRates[j*hoppingSiteNumber+i]*enhance::mediumFastExp(-deltaEnergies[j*hoppingSiteNumber+i]);
            }
            else if (deltaEnergies[i*hoppingSiteNumber+j] > 0){
                rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j]*enhance::mediumFastExp(-deltaEnergies[i*hoppingSiteNumber+j]);
                rates[j*hoppingSiteNumber+i]=baseRates[i*hoppingSiteNumber+j];
            }
            else{
                rates[i*hoppingSiteNumber+j]=baseRates[i*hoppingSiteNumber+j];
                rates[j*hoppingSiteNumber+i]=baseRates[i*hoppingSiteNumber+j];
            }
            constantRatesSumPart+=rates[i*hoppingSiteNumber+j];
            constantRatesSumPart+=rates[j*hoppingSiteNumber+i];
        }
    }

    DEBUG_FUNC_END
}


mfem::GridFunction System::getPotential() const{
    DEBUG_FUNC_START
    DEBUG_FUNC_END
    return *(finEle->solutionVector);
}


void System::findSwap(){
    DEBUG_FUNC_START

    double rndNumber=enhance::random_double(0,ratesSum);
    double partRatesSum=0;

    // from acc ... 
    for(int i=0;i<acceptorNumber;i++){
        if (occupation[i]){
            // .. to acc
            for(int const & j : hoppingPartnersAcceptors[i]){
                if (not occupation[j]){
                    partRatesSum+=rates[i*hoppingSiteNumber+j];
                    // std::cout<<"i "<<i<<" j: "<<j<<" rate: "<<rates[i*hoppingSiteNumber+j]<<std::endl;
                    if(partRatesSum > rndNumber){
                        lastSwapped1=i;
                        lastSwapped2=j;
                        goto foundSwap;
                    }
                }
            }
            // .. to electrode
            for(int const & j : hoppingPartnersElectrodes[i]){
                partRatesSum+=rates[i*hoppingSiteNumber+j];
                // std::cout<<"i "<<i<<" j: "<<j<<" rate: "<<rates[i*hoppingSiteNumber+j]<<std::endl;
                if(partRatesSum > rndNumber){
                    lastSwapped1=i;
                    lastSwapped2=j;
                    goto foundSwap;
                }
            }
        }
    }
    // from electrode ... 
    for(int i=acceptorNumber;i<hoppingSiteNumber;i++){
        // .. to acc
        for(int const & j : hoppingPartnersAcceptors[i]){
            if (not occupation[j]){
                partRatesSum+=rates[i*hoppingSiteNumber+j];
                // std::cout<<"i "<<i<<" j: "<<j<<" rate: "<<rates[i*hoppingSiteNumber+j]<<std::endl;
                if(partRatesSum > rndNumber){
                    lastSwapped1=i;
                    lastSwapped2=j;
                    goto foundSwap;
                }
            }
        }
        // .. to electrode
        for(int const & j : hoppingPartnersElectrodes[i]){
            partRatesSum+=rates[i*hoppingSiteNumber+j];
            // std::cout<<"i "<<i<<" j: "<<j<<" rate: "<<rates[i*hoppingSiteNumber+j]<<std::endl;
            if(partRatesSum > rndNumber){
                lastSwapped1=i;
                lastSwapped2=j;
                goto foundSwap;
            }
        }
    }
    // std::cout<<"no swapp found! ratesSum: "<<ratesSum<<" partRatesSum: "<<partRatesSum<<" rndNumber: "<<rndNumber<<std::endl;

    foundSwap:;
    // std::cout<<"rates: "<<std::endl;
    // for(int k=0; k<hoppingSiteNumber*hoppingSiteNumber;k++){
    //     std::cout<<rates[k]<<" ";
    // }
    // std::cout<<"ratesSum: "<<ratesSum<<" rndNumber: "<<rndNumber<<std::endl;

    DEBUG_FUNC_END
}

void System::findSwapBS(){
    DEBUG_FUNC_START
    double rndNumber=enhance::random_double(0,ratesSum);
    int l=0, r=hoppingSiteNumber*hoppingSiteNumber+1;
    int mid=-1;


    //binary search algorithm
    while (true){
        mid=(l+r)/2;
        // std::cout<<mid<<" ";

        if((*partRatesSumList)[mid]>rndNumber){
            if((*partRatesSumList)[mid-1]<rndNumber){
                lastSwapped1=(mid-1)/hoppingSiteNumber;
                lastSwapped2=(mid-1)%hoppingSiteNumber;
                // std::cout<<"swapped2 "<<lastSwapped1<<" "<<lastSwapped2<<" k: "<<mid-1<<std::endl;
                break;
            }
            else{
                r=mid;
            }
        }
        else if ((*partRatesSumList)[mid+1]>rndNumber){
            lastSwapped1=(mid)/hoppingSiteNumber;
            lastSwapped2=(mid)%hoppingSiteNumber;
            // std::cout<<"swapped2 "<<lastSwapped1<<" "<<lastSwapped2<<" k: "<<mid<<std::endl;
            break;
        }
        else{
            l=mid;
        }
    }

    // std::cout<<"rates: "<<std::endl;
    // for(int k=0; k<hoppingSiteNumber*hoppingSiteNumber;k++){
    //     std::cout<<rates[k]<<" ";
    // }
    // std::cout<<std::endl;
    // std::cout<<"partRatesSumList: "<<std::endl;
    // for(int k=0; k<hoppingSiteNumber*hoppingSiteNumber+1;k++){
    //     std::cout<<(*partRatesSumList)[k]<<" ";
    // }
    // std::cout<<std::endl;
    // std::cout<<"ratesSum: "<<ratesSum<<" rndNumber: "<<rndNumber<<std::endl;

    DEBUG_FUNC_END
}

void System::updateAfterSwap(){
    DEBUG_FUNC_START

    currentCounter[lastSwapped1]--;
    currentCounter[lastSwapped2]++;

    #ifdef SWAPTRACKER
    swapTrackFile<<lastSwapped1<<";"<<lastSwapped2<<std::endl; // swapTracker
    #endif

    if (lastSwapped1 < acceptorNumber){ //last swapped1 = acceptor, else electrode
        occupation[lastSwapped1]=false;
        //update energy
        for(int const & j : interactionPartners[lastSwapped1]){ 
            energies[j]+=pairEnergies[lastSwapped1*acceptorNumber+j];
        }
    }

    if (lastSwapped2 < acceptorNumber){ //last swapped2 = acceptor
        occupation[lastSwapped2]=true;
        for(int const & j : interactionPartners[lastSwapped2]){ 
            energies[j]-=pairEnergies[lastSwapped2*acceptorNumber+j];
        }
    }

    // std::cout<<occupation<<std::endl;

    DEBUG_FUNC_END
}




void System::run(int steps){
    DEBUG_FUNC_START


    if (storingMode) {
        for(int i=0; i<steps;i++){
            // check if memory limit is exceeded
            if (storeKnownStates & (i%1000 ==0) & (((hoppingSiteNumber*hoppingSiteNumber+2)*8*knownRatesSum->size()) >= (parameterStorage->parameters.at("memoryLimit")*1e6))){
                storeKnownStates=false;
                // std::cout<<"memory limit exceeded, stopping to store states"<<std::endl;
            }
            updateRatesStoringMode();
            if (ratesInMemory){
                findSwapBS();
            }
            else{
                findSwap();
            }
            updateAfterSwap();
            increaseTime();
        }
    }
    else {
        for(int i=0; i<steps;i++){
            updateRates();
            findSwap();
            updateAfterSwap();
            increaseTime();
        }
    }
    
    // std::cout<<"done! curr: "<<" "<<currentCounter[int(parameterStorage->parameters.at("outputElectrode")+parameterStorage->parameters["acceptorNumber"])]/time <<std::endl;
    DEBUG_FUNC_END
}


void System::increaseTime(){
    DEBUG_FUNC_START

    time+=std::log(enhance::random_double(0,1))/(-1*ratesSum); //avoid zero
    // std::cout<<"time "<< time <<std::endl;

    DEBUG_FUNC_END
}

void System::reset(){
    DEBUG_FUNC_START

    time=0;
    for (int i = 0; i < hoppingSiteNumber; i++){
        currentCounter[i] = 0;
    }

    DEBUG_FUNC_END
}

void System::resetStoredStates(){
    DEBUG_FUNC_START

    knownRatesSum->clear();
    konwnPartRatesSumList->clear();

    DEBUG_FUNC_END
}
