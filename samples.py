import numpy as np
import matplotlib.pyplot as plt
import math

samples = np.fromfile("resampled.snd",  dtype=np.int16)


#samples = samples[20000*4:20000*6]

# Creating a numpy array
X = np.array(range(0, len(samples)))
Y = np.array(samples)


import matplotlib.pyplot as plt

fig = plt.figure()
ax1 = fig.add_subplot(111)

#ax1.plot(X, Y, color='green', linestyle='solid', linewidth=2)


def chunks(lst, n):
    """Yield successive n-sized chunks from lst."""
    for i in range(0, len(lst), n):
        yield np.int32(lst[i:i + n])

X2 = []
Y2 = [] # sum power
Y3 = [] # rms
Y4 = [] # window
xpos = 0

av = 0
av_dv = 0

chunk_size = 512
for x in chunks(samples,chunk_size):
  if len(x) < chunk_size:
      break

  sum = 0
  for xx in x:
      sum += xx * xx

  av += sum / 1e5
  av_dv += 1

  X2.append( xpos/chunk_size )
  Y2.append( sum / 1e5)
  Y3.append( sum / 1e5)
  xpos += chunk_size
  

av /= av_dv

for i,y in enumerate(Y2):
    Y2[i] = max(0, y-av)

ax1.plot(X2, Y2, color='red', linestyle='solid', linewidth=2)


fsamp = 20000 / chunk_size
max_bpm = 0
max_bpm_val = 0
max_phase = 0


bx = []
by = []

hump_width = 8
bump = []
for k in range(hump_width):
    bump.append( math.floor( 255 * math.sin(2 * math.pi * (k+0.5) / hump_width / 2) + 0.5) )

fig = plt.figure()
ax1 = fig.add_subplot(111)
ax1.plot(range(len(bump)), bump, color='red', linestyle='solid', linewidth=2)


#for bpm in range( 80, 181, 1 ):
for bpm in range( 100, 141, 1 ):

    bump_inc = bpm / 60 / fsamp
    bump_width = math.floor(2 / bump_inc)
    print(bump_inc, bump_width)

    Y4 = []

    max_p_val = 0
    max_p_pos = 0

    ki = []
    kj = []
    kk = []

 #   fig = plt.figure()
 #   ax1 = fig.add_subplot(111)

    for phase in range(0, bump_width*2,1):

        samp_sum = 0
        bump_sum = 0
        kj= []

        for i,x in enumerate(X2):
            bump_ = math.sin( 2 * math.pi * (i+phase)*bump_inc)# * 30000
            bump_ = max( 0, bump_ )

        #    print(Y2[i], bump, samp_sum)
            samp_sum += (Y2[i]) * bump_
            bump_sum += bump_

            ki.append( Y2[i] )
            kj.append( samp_sum/100)
            #kj.append(10000*bump)
            kk.append((Y2[i])*bump_)

        #print(bpm, phase, samp_sum, bump_sum, samp_sum/bump_sum)
        
        #ax1.plot(range(len(ki)), ki, color='red', linestyle='solid', linewidth=2)
    #    ax1.plot(range(len(kj)), kj, color='green', linestyle='solid', linewidth=2)
        #ax1.plot(range(len(kk)), kk, color='blue', linestyle='solid', linewidth=2)
        #plt.show()
        
        #exit()
        #break
        samp_sum /= bump_sum # normalise
        if samp_sum > max_p_val:
            max_p_val = samp_sum
            max_p_pos = phase

    print(bpm)
    #plt.show()
    print(bpm, max_p_val,max_p_pos)

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
print(int( max_phase), int(len(X2)-hump_width/2), int(hump_stride))

for x in range(int(hump_width/2+max_phase), int(len(X2)-hump_width/2), int(hump_stride)):
    for xx,h in enumerate(bump):
        hx.append((x-hump_width/2+xx))
        hy.append(h)

fig = plt.figure()
ax1 = fig.add_subplot(111)
ax1.plot(hx, hy, color='red', linestyle='solid', linewidth=2)


for i,x in enumerate(X2):
    bump = math.sin( 2 * math.pi * (i+max_phase) *max_bpm / 60 / fsamp) * 30000
    bump = max( 0, bump )
    Y4.append( bump*3 )

fig = plt.figure()
ax1 = fig.add_subplot(111)
ax1.plot(X2, Y3, color='red', linestyle='solid', linewidth=2)
ax1.plot(X2, Y4, color='green', linestyle='solid', linewidth=2)

fig = plt.figure()
ax1 = fig.add_subplot(111)
ax1.plot(bx, by, color='red', linestyle='solid', linewidth=2)



plt.show()


