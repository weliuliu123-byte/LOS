set terminal png size 1400,700
set output '~/embedded_lab3/logs/latency_histogram.png'

set title 'Cyclictest Latency Histogram - Standard Kernel Under Stress'
set xlabel 'Latency (microseconds)'
set ylabel 'Number of occurrences'

set xrange [0:500]
set grid
set key top right

plot \
'~/embedded_lab3/logs/hist_standard_stress.txt' using 1:2 with lines title 'Thread 0', \
'' using 1:3 with lines title 'Thread 1', \
'' using 1:4 with lines title 'Thread 2', \
'' using 1:5 with lines title 'Thread 3'
