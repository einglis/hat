import numpy as np
import matplotlib.pyplot as plt
import math
import time


def chunks(lst, n):
    """Yield successive n-sized chunks from lst."""
    for i in range(0, len(lst), n):

        x = np.int32(lst[i:i + n])
        if len(x) == n:
            yield x



fsamp = 20000
chunk_size = 512

window_length = fsamp * 2  # 2 seconds
window_length = chunk_size * math.ceil(window_length / chunk_size)

hump_width = 16
bpm_range = range( 80, 181, 1 )



all_samples = np.fromfile("resampled.snd", dtype=np.int16)


if 0:
    fig = plt.figure()
    plt.plot(all_samples)
    plt.title("raw samples")

if 1:

  fig = plt.figure()
  bpm_plot = fig.add_subplot(111)

 

  for samples in chunks(all_samples, window_length ):
    
    powers = []  


    for chunk in chunks(samples, chunk_size):
        sum = 0
        for x in chunk:
            sum += x * x
        powers.append( sum / chunk_size)

    average = 0
    for p in powers:
        average += p
    average /= len(powers)

    for i,p in enumerate(powers):
        powers[i] = max(0, p - average)



    #fig = plt.figure()
    #ax1 = fig.add_subplot(111)

    #ax1.plot(X, Y, color='green', linestyle='solid', linewidth=2)
    #ax1.plot(X2, Y2, color='red', linestyle='solid', linewidth=2)


    fsamp = 20000 / chunk_size
    max_bpm = 0
    max_bpm_val = 0
    max_phase = 0


    bx = []
    by = []


    bump = []
    for k in range(hump_width):
        bump.append( math.floor( 255 * math.sin(2 * math.pi * (k+0.5) / hump_width / 2) + 0.5) )

    # fig = plt.figure()
    # ax1 = fig.add_subplot(111)
    # ax1.plot(range(len(bump)), bump, color='red', linestyle='solid', linewidth=2)


    for bpm in bpm_range:
    #for bpm in range( 100, 141, 1 ):

        print(bpm)

        bump_inc = bpm / 60 / fsamp
        bump_width = math.floor(2 / bump_inc)
        #print(bump_inc, bump_width)

        Y4 = []

        max_p_val = 0
        max_p_pos = 0

        ki = []
        kj = []
        kk = []

    #    fig = plt.figure()
    #    ax1 = fig.add_subplot(111)


        num_phases = min(64, bump_width*2)
        #print(f"phase inc {bump_width*2 / num_phases}")
        for p in range(0, num_phases    ):
            
            phase = int(p * bump_width*2 / num_phases)
            #print(phase)

            samp_sum = 0
            bump_sum = 0
            kj= []

            for i,p in enumerate(powers):
                bump_ = math.sin( 2 * math.pi * (i+phase)*bump_inc)# * 30000
                bump_ = max( 0, bump_ )

            #    print(Y2[i], bump, samp_sum)
                samp_sum += (p) * bump_
                bump_sum += bump_

                ki.append( p )
                kj.append( samp_sum/100)
                #kj.append(10000*bump)
                kk.append((p)*bump_)

            #print(bpm, phase, samp_sum, bump_sum, samp_sum/bump_sum)
            
            #ax1.plot(range(len(ki)), ki, color='red', linestyle='solid', linewidth=2)
    #        ax1.plot(range(len(kj)), kj, color='green', linestyle='solid', linewidth=2)
            #ax1.plot(range(len(kk)), kk, color='blue', linestyle='solid', linewidth=2)
            #plt.show()
            
            #exit()
            #break
            samp_sum /= bump_sum # normalise
            if samp_sum > max_p_val:
                max_p_val = samp_sum
                max_p_pos = phase

    #    plt.title(bpm)
        #plt.show()
        #print(bpm, max_p_val,max_p_pos)

        if max_p_val > max_bpm_val:
            max_bpm_val = max_p_val
            max_phase = max_p_pos
            max_bpm = bpm

        bx.append(bpm)
        by.append(max_p_val)



    print(f"max {max_bpm} bpm at {max_phase}")


    bump_inc = max_bpm / 60 / fsamp
    bump_width = math.floor(2 / bump_inc)

    hump_stride = (60 / max_bpm) * fsamp


    max_phase = hump_stride * max_phase / (2*bump_width) # rescale
    print(f"phase now {max_phase}")



    hx = []
    hy = []
    print(int( max_phase), int(len(powers)-hump_width/2), int(hump_stride))

    i = 0
    x = int(max_phase)

    while x < int(len(powers)-hump_width) and i < 4:
        for xx,h in enumerate(bump):
            hx.append((x+xx-hump_width/2))
            hy.append(h*512) # scale purely for display

        i += 1
        x = int( i * (60 / max_bpm) * fsamp  + max_phase)



    for i,x in enumerate(X2):
        bump = math.sin( 2 * math.pi * (i+max_phase) *max_bpm / 60 / fsamp) * 30000
        bump = max( 0, bump )
        Y4.append( bump*3 )

    fig = plt.figure()
    ax1 = fig.add_subplot(111)
    ax1.plot(Y3, color='red', linestyle='solid', linewidth=2)
    ax1.plot(Y4, color='green', linestyle='solid', linewidth=2)
    ax1.plot(hy, color='blue', linestyle='solid', linewidth=2)

 
    bpm_plot.plot(bx, by, color='red', linestyle='solid', linewidth=2)


plt.show()


