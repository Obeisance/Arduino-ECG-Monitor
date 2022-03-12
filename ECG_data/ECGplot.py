import numpy as np
import matplotlib.pyplot as plt
import ECG_data as ekg

folder = 'C:/Users/David/Documents/Arduino/ecg_heart_monitor/'
file = '2022-02-27 thru 2022-02-28 ECG and pulse - has exercise, stretch, rest pulse, and treadmill run.txt'

ECG_logs = ekg.import_ECG_logs(folder+file)

for log in ECG_logs:
    if(log.isECG):
        #plot ECG data
        fig,ax = plt.subplots()
        ax.plot(log.time - np.min(log.time), log.ECG)
        ax.set_xlabel('Time [sec]')
        ax.set_ylabel('ECG [mV]')
        
        #check for time-stamp errors
        # fig,ax = plt.subplots()
        # ax.plot(np.arange(0,log.time.__len__()),log.time,)
    
    #plot pulse data
    pulse_mask = (log.pulse > 0)
    fig,ax = plt.subplots()
    ax.plot(log.time[pulse_mask]- np.min(log.time),log.pulse[pulse_mask],linestyle='None',marker='.')
    ax.set_xlabel('Time [sec]')
    ax.set_ylabel('Pulse [bpm]')
plt.show()