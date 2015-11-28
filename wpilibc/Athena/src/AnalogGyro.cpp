/*----------------------------------------------------------------------------*/
/* Copyright (c) FIRST 2008. All Rights Reserved.
 */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in $(WIND_BASE)/WPILib.  */
/*----------------------------------------------------------------------------*/

#include "AnalogGyro.h"
#include "AnalogInput.h"
//#include "NetworkCommunication/UsageReporting.h"
#include "Timer.h"
#include "WPIErrors.h"
#include "LiveWindow/LiveWindow.h"
#include <climits>
const uint32_t AnalogGyro::kOversampleBits;
const uint32_t AnalogGyro::kAverageBits;
constexpr float AnalogGyro::kSamplesPerSecond;
constexpr float AnalogGyro::kCalibrationSampleTime;
constexpr float AnalogGyro::kDefaultVoltsPerDegreePerSecond;

/**
 * Gyro constructor using the Analog Input channel number.
 *
 * @param channel The analog channel the gyro is connected to. Gyros
                      can only be used on on-board Analog Inputs 0-1.
 */
AnalogGyro::AnalogGyro(int32_t channel) :
    AnalogGyro(std::make_shared<AnalogInput>(channel)) {}

/**
 * Gyro constructor with a precreated AnalogInput object.
 * Use this constructor when the analog channel needs to be shared.
 * This object will not clean up the AnalogInput object when using this
 * constructor.
 * Gyros can only be used on on-board channels 0-1.
 * @param channel A pointer to the AnalogInput object that the gyro is connected
 * to.
 */
DEPRECATED(
    "Raw pointers are deprecated; consider calling the Gyro constructor with "
    "a channel number or passing a shared_ptr instead.")
AnalogGyro::AnalogGyro(AnalogInput *channel)
    : AnalogGyro(
          std::shared_ptr<AnalogInput>(channel, NullDeleter<AnalogInput>())) {}

/**
 * Gyro constructor with a precreated AnalogInput object.
 * Use this constructor when the analog channel needs to be shared.
 * This object will not clean up the AnalogInput object when using this
 * constructor
 * @param channel A pointer to the AnalogInput object that the gyro is
 * connected to.
 */
AnalogGyro::AnalogGyro(std::shared_ptr<AnalogInput> channel)
    : m_analog(channel) {
  if (channel == nullptr) {
    wpi_setWPIError(NullParameter);
  } else {
    if (!m_analog->IsAccumulatorChannel()) {
      wpi_setWPIErrorWithContext(ParameterOutOfRange,
                                 " channel (must be accumulator channel)");
      m_analog = nullptr;
      return;
    }

    m_voltsPerDegreePerSecond = kDefaultVoltsPerDegreePerSecond;
    m_analog->SetAverageBits(kAverageBits);
    m_analog->SetOversampleBits(kOversampleBits);
    float sampleRate =
        kSamplesPerSecond * (1 << (kAverageBits + kOversampleBits));
    m_analog->SetSampleRate(sampleRate);
    Wait(0.1);

    SetDeadband(0.0f);

    SetPIDSourceType(PIDSourceType::kDisplacement);

    HALReport(HALUsageReporting::kResourceType_Gyro, m_analog->GetChannel());
    LiveWindow::GetInstance()->AddSensor("Gyro", m_analog->GetChannel(), this);

    Calibrate();
  }
}

/**
 * Reset the gyro.
 * Resets the gyro to a heading of zero. This can be used if there is
 * significant
 * drift in the gyro and it needs to be recalibrated after it has been running.
 */
void AnalogGyro::Reset() { m_analog->ResetAccumulator(); }

/**
 * {@inheritDoc}
 */
void AnalogGyro::Calibrate() {
  if (StatusIsFatal()) return;

  m_analog->InitAccumulator();

  Wait(kCalibrationSampleTime);

  int64_t value;
  uint32_t count;
  m_analog->GetAccumulatorOutput(value, count);

  m_center = (uint32_t)((float)value / (float)count + .5);

  m_offset = ((float)value / (float)count) - (float)m_center;
  m_analog->SetAccumulatorCenter(m_center);
  m_analog->ResetAccumulator();
}

/**
 * Return the actual angle in degrees that the robot is currently facing.
 *
 * The angle is based on the current accumulator value corrected by the
 * oversampling rate, the
 * gyro type and the A/D calibration values.
 * The angle is continuous, that is it will continue from 360->361 degrees. This
 * allows algorithms that wouldn't
 * want to see a discontinuity in the gyro output as it sweeps from 360 to 0 on
 * the second time around.
 *
 * @return the current heading of the robot in degrees. This heading is based on
 * integration
 * of the returned rate from the gyro.
 */
float AnalogGyro::GetAngle() const {
  int64_t rawValue;
  uint32_t count;
  m_analog->GetAccumulatorOutput(rawValue, count);

  int64_t value = rawValue - (int64_t)((float)count * m_offset);

  double scaledValue = value * 1e-9 * (double)m_analog->GetLSBWeight() *
                       (double)(1 << m_analog->GetAverageBits()) /
                       (m_analog->GetSampleRate() * m_voltsPerDegreePerSecond);

  return (float)scaledValue;
}

/**
 * Return the rate of rotation of the gyro
 *
 * The rate is based on the most recent reading of the gyro analog value
 *
 * @return the current rate in degrees per second
 */
double AnalogGyro::GetRate() const {
  return (m_analog->GetAverageValue() - ((double)m_center + m_offset)) * 1e-9 *
         m_analog->GetLSBWeight() /
         ((1 << m_analog->GetOversampleBits()) * m_voltsPerDegreePerSecond);
}

/**
 * Set the gyro sensitivity.
 * This takes the number of volts/degree/second sensitivity of the gyro and uses
 * it in subsequent
 * calculations to allow the code to work with multiple gyros. This value is
 * typically found in
 * the gyro datasheet.
 *
 * @param voltsPerDegreePerSecond The sensitivity in Volts/degree/second
 */
void AnalogGyro::SetSensitivity(float voltsPerDegreePerSecond) {
  m_voltsPerDegreePerSecond = voltsPerDegreePerSecond;
}

/**
 * Set the size of the neutral zone.  Any voltage from the gyro less than this
 * amount from the center is considered stationary.  Setting a deadband will
 * decrease the amount of drift when the gyro isn't rotating, but will make it
 * less accurate.
 *
 * @param volts The size of the deadband in volts
 */
void AnalogGyro::SetDeadband(float volts) {
  int32_t deadband = volts * 1e9 / m_analog->GetLSBWeight() *
                     (1 << m_analog->GetOversampleBits());
  m_analog->SetAccumulatorDeadband(deadband);
}

std::string AnalogGyro::GetSmartDashboardType() const { return "AnalogGyro"; }