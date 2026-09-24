import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# Read CSV
df = pd.read_csv("test4.csv")

# ==========================================================
# Figure 1: Distance Comparison
# ==========================================================
plt.figure(figsize=(10, 5))

plt.plot(df["Tape"], marker='o', linewidth=2,
         color='tab:blue', label="Tape")

plt.plot(df["BOSCH"], marker='s', linewidth=2,
         color='tab:orange', label="BOSCH GLM 50-27 C")

plt.plot(df["Garmin_OffsetCorrected"], marker='^', linewidth=2,
         color='tab:green',
         label="Garmin LidarLite V4 LED (Offset Corrected)")

plt.xlabel("Measurement Number")
plt.ylabel("Distance (cm)")
plt.xticks(range(len(df)), range(1, len(df) + 1))
plt.grid(True, linestyle='--', alpha=0.4)
plt.legend(loc='best')

plt.tight_layout()
plt.show()

# ==========================================================
# Figure 2: Error Comparison
# ==========================================================
plt.figure(figsize=(10, 5))

# Plot error curves
plt.plot(df["Error_Tape"], marker='o', linewidth=2,
         color='tab:blue',
         label="Lidar Error (Reference: Tape)")

plt.plot(df["Error_BOSCH"], marker='s', linewidth=2,
         color='tab:orange',
         label="Lidar Error (Reference: BOSCH)")

# ----------------------------------------------------------
# Statistics
# ----------------------------------------------------------
mean_tape = df["Error_Tape"].mean()
mean_bosch = df["Error_BOSCH"].mean()

rmse_tape = np.sqrt(np.mean(df["Error_Tape"] ** 2))
rmse_bosch = np.sqrt(np.mean(df["Error_BOSCH"] ** 2))

print("====================================")
print("        Error Statistics")
print("====================================")
print(f"Tape Reference")
print(f"  Mean Error : {mean_tape:.2f} cm")
print(f"  RMSE       : {rmse_tape:.2f} cm")
print()
print(f"BOSCH Reference")
print(f"  Mean Error : {mean_bosch:.2f} cm")
print(f"  RMSE       : {rmse_bosch:.2f} cm")
print("====================================")

# ----------------------------------------------------------
# Mean Error Lines
# ----------------------------------------------------------
plt.axhline(mean_tape,
            color='red',
            linestyle='--',
            linewidth=2,
            label=f"Mean (Tape) = {mean_tape:.2f} cm")

plt.axhline(mean_bosch,
            color='purple',
            linestyle='--',
            linewidth=2,
            label=f"Mean (BOSCH) = {mean_bosch:.2f} cm")

# ----------------------------------------------------------
# RMSE Lines
# ----------------------------------------------------------
plt.axhline(rmse_tape,
            color='darkred',
            linestyle=':',
            linewidth=2,
            label=f"RMSE (Tape) = {rmse_tape:.2f} cm")

plt.axhline(rmse_bosch,
            color='darkviolet',
            linestyle=':',
            linewidth=2,
            label=f"RMSE (BOSCH) = {rmse_bosch:.2f} cm")

# ----------------------------------------------------------
# Plot formatting
# ----------------------------------------------------------
plt.xlabel("Measurement Number")
plt.ylabel("Error (cm)")
plt.xticks(range(len(df)), range(1, len(df) + 1))
plt.grid(True, linestyle='--', alpha=0.4)
plt.legend(loc='best')

plt.tight_layout()
plt.show()