#!/bin/bash
make clean | tee /tmp/tmper
make

for file in ../traces/*.bz2
do
    echo "Using $file"
    bunzip2 -kc $file |  ./predictor $* | tee -a /tmp/tmper
done

# Function to extract and calculate average misprediction rate
calculate_avg_misprediction_rate() {
    # Use awk to extract misprediction rates
    rates=$(grep "Misprediction Rate:" "$1" | awk '{print $3}' ORS=' ' | tr '\n' ' ' | sed "s/^/'/;s/$/'/")
    # Use Python to calculate the average
    python3 -c "
rates = [float(rate) for rate in $rates.split()]
print(f'{sum(rates) / len(rates):.3f}')
"
}

# Calculate and print the average misprediction rate
echo "Average Misprediction Rate: $(calculate_avg_misprediction_rate /tmp/tmper)"

