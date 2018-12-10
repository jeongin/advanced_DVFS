# advanced_DVFS

Linux shell script for advanced DVFS technique.

Set a maximum temperature, # of cores, maximum frequency of your devices. It predict the higest temperature of cores at different frequencies. The highest one is adopted for CPU frequency.

We use linear-regression model.
Linear-regression model was established using the following:

<pre><code>EXEC, IPC, FREQ, AFREQ, L3MPI, READ, WRITE,
INST, PhysIPC, INSTnom, Proc_Energy_(Joules), Total_Util, frequency </code></pre>
"Total_Util" is total utilization of cores.
"frequency" is current CPU frequency.
Other details are in PCM (Processor Counter Monitor) - https://github.com/opcm/pcm.
<hr/>
This script must be run with sudo privileges.

## Installation
1. Install PCM (Processor Counter Monitor)
<pre><code>git clone https://github.com/opcm/pcm.git </code></pre>

2. Download this project
<pre><code>git clone https://github.com/JeongIn/advanced_DVFS.git </code></pre>

3. Move "pcm-pred_model.cpp" and "advanced_DVFS" to "pcm" folder

4. Modify "Makefile"
<pre><code> EXE = pcm-pred_model.x </code></pre>


## How to Use
1. super user mode
<pre><code> su </code></pre>

2. Start "pcm-pred_model"
<pre><code>cd pcm/
./pcm-pred_model.x </code></pre>

3. Start "advanced_DVFS" with new terminal [This example will limit system temperature to 50 Celsius (8 cores, Maximum frequency: 4.2GHz)]
<pre><code>cd pcm/
./advanced_DVFS.sh 50 8 4200000 </code></pre> 

