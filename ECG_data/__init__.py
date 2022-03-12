import numpy as np

class ECG_log():
    time = []
    ECG = []
    pulse = []
    isECG = False
    
    def __init__(self):
        time = []
        ECG = []
        pulse = []
        isECG = False
        return None
    
    def set_ECG(self, new_ECG_data):
        self.ECG = np.array(new_ECG_data)
        return None
    
    def set_time(self, new_time_data):
        self.time = np.array(new_time_data)
        return None
    
    def set_pulse(self, new_pulse_data):
        self.pulse = np.array(new_pulse_data,dtype=np.int)
        return None
    
    def set_type(self, isECG):
        self.isECG = isECG
        return None
    

def import_ECG_logs(filename):
    list_of_ECG_logs = []
    
    with open(filename) as file:
        first_line = True
        ECG_data = []
        time_data = []
        pulse_data = []
        non_zero_ecg = False
        for line in file:
            index = line.find('Time') #label/header of new log
            if(index != -1):
                #print(line.strip())
                #we're on a header line
                if(first_line != True):
                    ECG = ECG_log()
                    ECG.set_ECG(ECG_data)
                    ECG.set_time(time_data)
                    ECG.set_pulse(pulse_data)
                    ECG.set_type(non_zero_ecg)
                    
                    list_of_ECG_logs.append(ECG)
                    
                    ECG_data = []
                    time_data = []
                    pulse_data = []
                    non_zero_ecg = False
                first_line = False
            else:
                #we're on data
                data = np.array(line.strip()[:-1].split(','), dtype=np.float)
                time_data.append(data[0]/1e6)
                ECG_data.append(data[1])
                pulse_data.append(data[2])
                if((data[1] != 0) & (non_zero_ecg == False)):
                    non_zero_ecg = True
                    #print('ECG data nonzero')
        #the final log too          
        ECG = ECG_log()
        ECG.set_ECG(ECG_data)
        ECG.set_time(time_data)
        ECG.set_pulse(pulse_data)
        ECG.set_type(non_zero_ecg)
        list_of_ECG_logs.append(ECG)  
    file.close()
    return list_of_ECG_logs    