import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Read CSV
df = pd.read_csv("test1.csv")

# Extract data
distance = df["BOSCH_OffsetCorrected"]
error = df["Error"]

# Statistics
mean_error = error.mean()
std_error = error.std()

print(f"Mean Error = {mean_error:.2f} cm")
print(f"Standard Deviation = {std_error:.2f} cm")

# Create figure
plt.figure(figsize=(10, 6))

# Plot error
plt.plot(
    distance,
    error,
    'o-',
    color='royalblue',
    linewidth=2,
    markersize=6,
    label='Error'
)

# Plot mean line
plt.axhline(
    mean_error,
    color='red',
    linestyle='--',
    linewidth=2,
    label=f'Mean = {mean_error:.2f} cm'
)

# X-axis ticks every 5 cm
start = int(distance.min() // 5) * 5
end = int(np.ceil(distance.max() / 5)) * 5
plt.xticks(np.arange(start, end + 1, 5))

# Labels and title
plt.xlabel("BOSCH Offset Corrected Distance (cm)", fontsize=12)
plt.ylabel("Error (cm)", fontsize=12)
plt.title("BOSCH Offset Corrected Distance vs Error", fontsize=14)

# Grid
plt.grid(True, which='major', linestyle='--', alpha=0.5)

# Legend
plt.legend()

# Adjust layout
plt.tight_layout()

# Display plot
plt.show()