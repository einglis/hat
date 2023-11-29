import matplotlib.pyplot as plt
import math
import numpy as np


def chunks(lst, n):
    """Yield successive n-sized chunks from lst."""
    for i in range(0, len(lst), n):

        x = np.int32(lst[i:i + n])
        if len(x) == n:
            yield x


true_fsamp = 20000
chunk_size = 512
fsamp = true_fsamp / chunk_size

window_length = true_fsamp * 3    # x seconds
window_length = chunk_size * math.ceil(window_length / chunk_size)

bpm_range = range( 80, 181, 1 )
    # at 20kHz true_samp and 512 chunk, at 180 bpm, bumps are 13 samples apart
    # so bumps can't be more than this before they start overlapping, which is bad

bump_width = 8  # narrow is less comuptation and more discriminating
                # much more than 12 means they start overlapping at high BPMs
bump = []
for k in range(bump_width):
    bump.append( math.floor( 255 * math.sin(2 * math.pi * (k+0.5) / bump_width / 2) + 0.5) )


show_raw_samples  = False
show_bump_profile = False
show_phase_plots  = False # careful not to have too many BMPs and/or windows
show_bpm_results  = True
show_beat_points  = False


all_samples = np.fromfile("vain.snd", dtype=np.int16)

if show_raw_samples:
    fig = plt.figure()
    plt.plot(all_samples)
    plt.title("raw samples")

if show_bump_profile:
    fig = plt.figure()
    plt.plot(bump)
    plt.title("bump profile")

# temp = []
# for i,x in enumerate(all_samples):
#     if i % 10 != 0:
#         temp.append(x)
#     #temp.append(x)

# all_samples = temp


if 1:
 for kk in range(1):
  i_window = 1
  #for samples in chunks(all_samples, window_length ):
  for samples in chunks(all_samples, len(all_samples)):

    powers = []

    last_sum = 0
    for chunk in chunks(samples, chunk_size):
        sum = 0
        for x in chunk:
            sum += x * x

        powers.append(int((sum + last_sum)))
        last_sum = 0

    print(f"we have {len(powers)} powers")
    average = int(np.sum(powers) / len(powers))
    powers = [ max(0, p - average) for p in powers ]
    #powers = [ int(p)/1024 for p in powers ]




    bx = [] # bpm
    by = [] # sum at max phase
    by2 = [] # sum at max phase + 180
    by3 = [] # difference
    by4 = [] # difference
    by5 = [] # difference

    best_bpm = 0
    best_bpm_phase = 0
    best_bpm_sum = 0

    for bpm in bpm_range:

        max_phases = int((60 / bpm) * fsamp)   # approx interval between bumps at this BPM
        #print(f"at {bpm} bpm, max phases is {max_phases}")

        if show_phase_plots:
            phase_plot = plt.figure().add_subplot(111)

        phase_values = []

        num_phases = min(16, max_phases)  ## XXXEDD: this logic may be overkill
        for p in range(0, num_phases):

            phase = int((max_phases / num_phases) * p)

            samp_sum = 0
            sums = [] # history for debugging
            sums2 = [0] * len(powers)

            temp = [0] * bump_width

            i_bump = 0

            aa = [0] * len(powers)
            bb = [0] * len(powers)
            cc = [0] * len(powers)
            bump_sum = 0

            while i_bump < 100:
                b_start = int(i_bump * (60 / bpm) * fsamp + phase)
                b_start_fl = (i_bump * (60 / bpm) * fsamp + phase)
                #print(f"{bpm}, {phase}, bump {i_bump} at {b_start}")

                if b_start+bump_width > len(powers):
                    break

                # bump = []
                # for k in range(bump_width):
                #     bump.append( math.sin(2 * math.pi * (k+0.5) / bump_width / 2 + b_start_fl - phase)  )


                for i,b in enumerate(bump):
                    samp_sum += b * powers[b_start + i] * b# * powers[b_start + i]
                    aa[b_start + i] = powers[b_start + i]
                    bb[b_start + i] = b*1e10/255
                    cc[b_start + i] = b * powers[b_start + i]/255
                    sums2[b_start + i] = samp_sum / 2.5e4
                    sums.append(samp_sum/1e10)
                    bump_sum += b

                i_bump += 1

            sl = 0
            for i,s in enumerate(sums2):
                if s > 0:
                    sl = s
                else:
                    sums2[i] = sl

            zz = []
            for i in range(len(powers)):
                zz.append(max(0, math.sin(2 * math.pi * (i+4.5) / (60 / bpm * fsamp)    ) * 11e6 ) )

            # if (bpm == 88 and phase == 24) :#or (bpm == 133 and phase == 6)    :
            #     beat_plot = plt.figure().add_subplot(111)
            #     beat_plot.plot(powers, color='red', linestyle='solid', linewidth=2)
            #     beat_plot.plot(sums2, color='green', linestyle='solid', linewidth=2)
            #     beat_plot.plot(cc, color='blue', linestyle='solid', linewidth=2)
            #     beat_plot.plot(bb, color='orange', linestyle='solid', linewidth=2)
            #    # beat_plot.plot(zz, color='magenta', linestyle='solid', linewidth=2)
            #     plt.title(f"{bpm} bpm, phase {phase}")
            # #plt.show()
            # exit()

            if 0:#show_phase_plots:
                phase_plot.plot(sums, color='orange', linestyle='solid', linewidth=2)
                plt.title(f"{bpm}")


            phase_values.append(samp_sum / bump_sum)  # normalise




        best_phase = 0
        best_phase_sum = 0

        if show_phase_plots:
            phase_plot.plot(phase_values, color='orange', linestyle='solid', linewidth=2)
            plt.title(f"{bpm}")

        avavav = 0

        for i,val in enumerate(phase_values):
            avavav += val
            if val > best_phase_sum:
                best_phase_sum = val
                best_phase = i

        avavav /= len(phase_values)

        print(f"this {bpm} bpm best at {best_phase}; 180% is {(best_phase + num_phases//2)%num_phases}")

        bx.append(bpm)
        by.append(best_phase_sum)
        by2.append(min(phase_values))
        by3.append(max(0, best_phase_sum-phase_values[ (best_phase + num_phases//2)%num_phases]))
        by4.append(max(0, best_phase_sum-min(phase_values)))
        by5.append(max(0, max(0,best_phase_sum-min(phase_values)-avavav)))

        if best_phase_sum > best_bpm_sum:
            best_bpm_sum = best_phase_sum
            best_bpm_phase = best_phase
            best_bpm = bpm


    print(f"max {best_bpm} bpm at {best_bpm_phase}")


    if show_beat_points:

        beats = [0] * len(powers)

        x = best_bpm_phase + bump_width/2
        while x < len(powers):
            beats[int(x)] = 256
            x += (60 / best_bpm) * fsamp


        max_power = max(powers)
        powers = [ p / max_power * 256 for p in powers ] # normalise for display

        beat_plot = plt.figure().add_subplot(111)
        beat_plot.plot(beats, color='green', linestyle='solid', linewidth=2)
        beat_plot.plot(powers, color='red', linestyle='solid', linewidth=2)
        plt.title(f"beats, window {i_window}")

    if show_bpm_results:

        bpm_plot = plt.figure().add_subplot(111)
        bpm_plot.plot(bx, by, color='green', linestyle='solid', linewidth=2)
        bpm_plot.plot(bx, by2, color='red', linestyle='solid', linewidth=2)
        bpm_plot.plot(bx, by3, color='blue', linestyle='solid', linewidth=2)
        bpm_plot.plot(bx, by4, color='magenta', linestyle='solid', linewidth=2)
        bpm_plot.plot(bx, by5, color='orange', linestyle='solid', linewidth=2)
        plt.title(f"bpm response, window {i_window}")

    i_window += 1


plt.show()


