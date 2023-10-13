import functools
import matplotlib.pyplot as plt
import math
import numpy as np
import time


def chunks(lst, n):
    """Yield successive n-sized chunks from lst."""
    for i in range(0, len(lst), n):

        x = np.int32(lst[i:i + n])
        if len(x) == n:
            yield x


true_fsamp = 20000
chunk_size = 512
fsamp = true_fsamp / chunk_size

window_length = true_fsamp * 2  # 2 seconds
window_length = chunk_size * math.ceil(window_length / chunk_size)

bpm_range = range( 80, 181, 1 )
    # at 20kHz true_samp and 512 chunk, at 180 bpm, bumps are 13 samples apart
    # so bumps can't be more than this before they start overlapping, which is bad

bump_width = 8  # narrow is less comuptation and more discriminating
                # much more than 12 means they start overlapping at high BPMs
bump = []
for k in range(bump_width):
    bump.append( math.floor( 255 * math.sin(2 * math.pi * (k+0.5) / bump_width / 2) + 0.5) )
bump_sum = np.sum(bump)


show_raw_samples  = False
show_bump_profile = False



all_samples = np.fromfile("resampled.snd", dtype=np.int16)

if show_raw_samples:
    fig = plt.figure()
    plt.plot(all_samples)
    plt.title("raw samples")

if show_bump_profile:
    fig = plt.figure()
    plt.plot(bump)
    plt.title("bump profile")


if 1:



  for samples in chunks(all_samples, len(all_samples)):#window_length ):
    
    powers = []  

    for chunk in chunks(samples, chunk_size):
        sum = 0
        for x in chunk:
            sum += x * x
        powers.append(int(sum / chunk_size))

    average = int(np.sum(powers) / len(powers))
    powers = [ max(0, p - average) for p in powers ]



 

    bx = [] # bpm
    by = [] # confidence

    max_bpm = 0
    max_bpm_value = 0
    max_bpm_phase = 0

    for bpm in bpm_range:

        bump_inc = bpm / 60 / fsamp
        max_phases = int((60 / bpm) * fsamp)   # approx interval between bumps at this BPM
        #print(f"at {bpm} bpm, max phases is {max_phases}")


        max_p_val = 0
        max_p_pos = 0

     

#        phase_plot = plt.figure().add_subplot(111)

        num_phases = min(8, max_phases)  ## XXXEDD: this logic may be overkill
        for p in range(0, num_phases):
            
            if (num_phases == max_phases):
                phase = p
            else:
                phase = int((max_phases / num_phases) * p)


            samp_sum = 0

            kx = []
            ki = []
            kj = []
            kk = []
            km = []

            i_bump = 0
            while i_bump < 100:
                b_start = int(i_bump * (60 / bpm) * fsamp + phase)
                print(f"{bpm}, {phase}, bump {i_bump} at {b_start}")

                if b_start+bump_width > len(powers):
                    break

                for i,b in enumerate(bump):
                    samp_sum += b * powers[b_start + i]
                    
                    kx.append(b_start + i)
                    kj.append(b*1e5)
                    kk.append(b * powers[b_start + i]/256)
                    km.append(samp_sum/2000)

                i_bump += 1

            if 0:
                fig = plt.figure()
                conv_plot = fig.add_subplot(111)
                conv_plot.plot(powers, color='red', linestyle='solid', linewidth=2)
                conv_plot.plot(kx,kj, color='green', linestyle='solid', linewidth=2)
                conv_plot.plot(kx,kk, color='blue', linestyle='solid', linewidth=2)
                conv_plot.plot(kx,km, color='orange', linestyle='solid', linewidth=2)
                plt.ylim(0, 2.6e7)

#            phase_plot.plot(kx,km, color='orange', linestyle='solid', linewidth=2)
#            plt.title(f"{bpm}")

            #plt.show()
        
            # kx = []  
            # ki = []
            # kj = []
            # kk = []
            # km = []

            # samp_sum = 0
            # bump_sum = 0
        
            # for i,p in enumerate(powers):
            #     bump_ = math.sin( 2 * math.pi * (i+phase)*bump_inc)# * 30000
            #     bump_ = int(max( 0, bump_ )*255)

            # #    print(Y2[i], bump, samp_sum)
            #     samp_sum += (p) * bump_
            #     bump_sum += bump_

            
           


            #     ki.append( p )
            #     #kj.append( samp_sum/100)
            #     kk.append((p)*bump_/256)
            #     kj.append(1e5*bump_)
            #     km.append(samp_sum/2000)



            # #print(bpm, phase, samp_sum, bump_sum, samp_sum/bump_sum)
            # fig = plt.figure()
            # ax1 = fig.add_subplot(111)
            # ax1.plot(range(len(ki)), ki, color='red', linestyle='solid', linewidth=2)
            # ax1.plot(range(len(kj)), kj, color='green', linestyle='solid', linewidth=2)
            # ax1.plot(range(len(kk)), kk, color='blue', linestyle='solid', linewidth=2)
            # ax1.plot(range(len(km)),km, color='orange', linestyle='solid', linewidth=2)
            # plt.ylim(0, 2.6e7)
            # plt.show()
            
            #exit()
            #break
            samp_sum /= bump_sum * i_bump # normalise
            if samp_sum > max_p_val:
                max_p_val = samp_sum
                max_p_pos = phase

    #    plt.title(bpm)
        #plt.show()
        #print(bpm, max_p_val,max_p_pos)

        if max_p_val > max_bpm_value:
            max_bpm_value = max_p_val
            max_bpm_phase = max_p_pos
            max_bpm = bpm

        bx.append(bpm)
        by.append(max_p_val)



    print(f"max {max_bpm} bpm at {max_bpm_phase}")


    bump_inc = max_bpm / 60 / fsamp
    bump_width = math.floor(2 / bump_inc)

    hump_stride = (60 / max_bpm) * fsamp


    #max_bpm_phase = hump_stride * max_bpm_phase / (2*bump_width) # rescale
    print(f"phase now {max_bpm_phase}")



    hx = []
    hy = []
    print(int( max_bpm_phase), int(len(powers)-bump_width/2), int(hump_stride))

    i = 0
    x = int(max_bpm_phase)

    while x < int(len(powers)-bump_width) and i < 4:
        for xx,h in enumerate(bump):
            hx.append((x+xx-bump_width/2))
            hy.append(h)

        i += 1
        x = int( i * (60 / max_bpm) * fsamp  + max_bpm_phase)



    for i,x in enumerate(powers):
        bump__ = math.sin( 2 * math.pi * (i+max_bpm_phase) *max_bpm / 60 / fsamp)
        bump__ = max( 0, bump__ * 256 )

    ax1 = plt.figure().add_subplot(111)

    max_power = max(powers)
    powers = [ p / max_power * 256 for p in powers ]

    ax1.plot(powers, color='red', linestyle='solid', linewidth=2)
    ax1.plot(bump__, color='green', linestyle='solid', linewidth=2)
    ax1.plot(hy, color='blue', linestyle='solid', linewidth=2)

    bpm_plot = plt.figure().add_subplot(111)
    bpm_plot.plot(bx, by, color='red', linestyle='solid', linewidth=2)



plt.show()


