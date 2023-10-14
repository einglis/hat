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
bump_sum = np.sum(bump)


show_raw_samples  = False
show_bump_profile = False
show_phase_plots  = False # careful not to have too many BMPs and/or windows
show_bpm_results  = True
show_beat_points  = True


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
 for kk in range(1):
  i_window = 1
  #for samples in chunks(all_samples, window_length ):
  for samples in chunks(all_samples, len(all_samples)):
    
    powers = []  

    for chunk in chunks(samples, chunk_size):
        sum = 0
        for x in chunk:
            sum += x * x
        powers.append(int(sum / chunk_size))

    average = int(np.sum(powers) / len(powers))
    powers = [ max(0, p - average) for p in powers ]



 

    bx = [] # bpm
    by = [] # sum at max phase

    max_bpm_sum = 0
    max_bpm_phase = 0
    max_bpm = 0

    for bpm in bpm_range:

        max_phases = int((60 / bpm) * fsamp)   # approx interval between bumps at this BPM
        #print(f"at {bpm} bpm, max phases is {max_phases}")

        if show_phase_plots:
            phase_plot = plt.figure().add_subplot(111)

        max_sum = 0
        max_phase = 0

        num_phases = min(8, max_phases)  ## XXXEDD: this logic may be overkill
        for p in range(0, num_phases):
            
            if (num_phases == max_phases):
                phase = p
            else:
                phase = int((max_phases / num_phases) * p)

            samp_sum = 0
            sums = [] # history for debugging

            temp = [0] * bump_width

            i_bump = 0
            while i_bump < 6:
                b_start = int(i_bump * (60 / bpm) * fsamp + phase)
                #print(f"{bpm}, {phase}, bump {i_bump} at {b_start}")

                if b_start+bump_width > len(powers):
                    break

                for i in range(bump_width):
                    temp[i] += powers[b_start + i]

                i_bump += 1

            for i,b in enumerate(bump):
                samp_sum += b * temp[i]
                sums.append(samp_sum)

            samp_sum /= bump_sum * i_bump # normalise

            if samp_sum > max_sum:
                max_sum = samp_sum
                max_phase = phase


            if show_phase_plots:
                phase_plot.plot(sums, color='orange', linestyle='solid', linewidth=2)
                plt.title(f"{bpm}")


        if max_sum > max_bpm_sum:
            max_bpm_sum = max_sum
            max_bpm_phase = max_phase
            max_bpm = bpm

        bx.append(bpm)
        by.append(max_sum)

    print(f"max {max_bpm} bpm at {max_bpm_phase}")




    if show_beat_points:

        beats = [0] * len(powers)

        x = max_bpm_phase + bump_width/2
        while x < len(powers):
            beats[int(x)] = 256
            x += (60 / max_bpm) * fsamp


        max_power = max(powers)
        powers = [ p / max_power * 256 for p in powers ] # normalise for display

        beat_plot = plt.figure().add_subplot(111)
        beat_plot.plot(beats, color='green', linestyle='solid', linewidth=2)
        beat_plot.plot(powers, color='red', linestyle='solid', linewidth=2)
        plt.title(f"beats, window {i_window}")

    if show_bpm_results:

        bpm_plot = plt.figure().add_subplot(111)
        bpm_plot.plot(bx, by, color='red', linestyle='solid', linewidth=2)
        plt.title(f"bpm response, window {i_window}")

    i_window += 1


plt.show()


