#include <Arduino.h>
#include <TensorFlowLite.h>
#include "anomaly_detection_model.h" // Contains the converted model array

#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <Arduino_APDS9960.h>

#define BUTTON_PIN D6 // Pin for the hardware button

/*#### Prepare Tensorflow Data structures ####*/
tflite::MicroErrorReporter micro_error_reporter;  // Object for handling and reporting errors during TensorFlow Lite operations
tflite::ErrorReporter* error_reporter = &micro_error_reporter;  // Pointer to the error reporter, used by the interpreter to log errors
//tflite::AllOpsResolver micro_op_resolver;  // Resolver that includes all TensorFlow Lite operations available for use in the model
static tflite::MicroMutableOpResolver<7> micro_op_resolver;
tflite::MicroInterpreter* interpreter;  // Pointer to the MicroInterpreter, which runs the TensorFlow Lite model on the microcontroller
TfLiteTensor* input;  // Pointer to the input tensor, where the model’s input data will be stored
TfLiteTensor* output;  // Pointer to the output tensor, where the model’s prediction results will be stored

// Define model and input data
constexpr int tensor_arena_size = 2 * 1024;  // Size of memory (in bytes) to allocate for model execution, including input/output tensors
alignas(16) uint8_t tensor_arena[tensor_arena_size];  // Memory buffer aligned to 16 bytes, used to store tensors (input/output) and intermediate computations during model inference



/*#### Prepare Variables ####*/
const int seq_length = 8;  // Length of the sequence
const int num_samples = 40; // Number of data points in a measurement series
float measurement_data[num_samples][2]; // Array for storing the measurement series
int sample_index = 0;

// Normalization parameters (from the Python script)
float min_timestamp = 0;
float max_timestamp = 9750;
float min_intensity = 62;
float max_intensity = 531;

void setup() {
  Serial.begin(9600);
  while(!Serial);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Configure the hardware button
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);

  Serial.println("Daa");

  // Initialize APDS-9960 sensor
  if (!APDS.begin()) {
    Serial.println("Error initializing APDS9960 sensor!");
    while (1);
  }

  // Load model
  const tflite::Model* model = tflite::GetModel(anomaly_detection_model_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model provided is schema version not equal to supported version.");
    while (1);
  }

  micro_op_resolver.AddConv2D();
  micro_op_resolver.AddFullyConnected();
  micro_op_resolver.AddRelu();
  micro_op_resolver.AddDepthwiseConv2D();
  micro_op_resolver.AddMaxPool2D();
  micro_op_resolver.AddReshape();
  micro_op_resolver.AddMean();  // Add operations your model requires


  // Initialize interpreter
  static tflite::MicroInterpreter static_interpreter(model, micro_op_resolver, tensor_arena, tensor_arena_size);
  interpreter = &static_interpreter;

  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    Serial.println("AllocateTensors() failed");
    while (1);
  }

  input = interpreter->input(0);
  output = interpreter->output(0);

  Serial.println("System ready. Press the button to start a new measurement series.");
}

void setLEDColor(int red, int green, int blue) {
  digitalWrite(LEDR, red);
  digitalWrite(LEDG, green);
  digitalWrite(LEDB, blue);
}


void loop() {
  setLEDColor(HIGH, HIGH, HIGH);
  // Wait for button press to start a new measurement series
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(2000); // Debounce to prevent accidental multiple readings due to button bounce
    setLEDColor(HIGH, LOW, HIGH);
    sample_index = 0;
    Serial.println("Starting new measurement series...");

    // Capture a complete measurement series
    // This loop gathers ambient light data and timestamps over a series of samples (num_samples)
    while (sample_index < num_samples) {
      int r, g, b, a;
      if (APDS.colorAvailable()) { 
        APDS.readColor(r, g, b, a);  // Read the Red, Green, Blue, and Ambient light intensity from the sensor

        // Fill the timestamp and ambient light intensity
        measurement_data[sample_index][0] = sample_index * 250; // Record the timestamp (each sample is 250ms apart)
        measurement_data[sample_index][1] = a;  // Store the ambient light intensity

        Serial.print("Timestamp: ");
        Serial.print(measurement_data[sample_index][0]);  // Print timestamp in milliseconds
        Serial.print(" ms, Ambient Light Intensity: ");
        Serial.println(measurement_data[sample_index][1]);  // Print the ambient light intensity

        sample_index++;  
        delay(200);  // Wait 200ms before taking the next sample to maintain consistency (This is just rughly as the sensor needs also time for providing the new measurement. For this learning usecase it is fine and will lead to 230-270 ms in total)
      }
    }
    setLEDColor(HIGH, HIGH, HIGH);
    Serial.println("Measurement series complete. Checking for anomalies...");

    // Split data into sequences and check for anomalies
    // The collected data is processed in sequences to detect potential anomalies using the ML model
    for (int i = 0; i <= num_samples - seq_length; i++) {
      // Normalize the sequence
      // The model expects input data to be normalized, so the raw data is converted into a 0-1 range as we done it during the training of the model in python
      for (int j = 0; j < seq_length; j++) {
        // Normalize timestamp and ambient light intensity using min/max values and store the data in the input tensor
        input->data.f[j * 2] = (measurement_data[i + j][0] - min_timestamp) / (max_timestamp - min_timestamp);
        input->data.f[j * 2 + 1] = (measurement_data[i + j][1] - min_intensity) / (max_intensity - min_intensity);
      }

      // Run the model
      // The interpreter is invoked to process the input data using the loaded model. The results (predictions) are stored in the output tensor,
      if (interpreter->Invoke() != kTfLiteOk) {
        Serial.println("Invoke failed");
        while (1);
      }


      // Read the prediction
      // The model output is normalized, so we read the predicted normalized intensity from the output tensor.
      float predicted_normalized_intensity = output->data.f[1];

      // Recalculate the predicted intensity back to the original scale to be able to compare it with the actual intensity
      float predicted_intensity = predicted_normalized_intensity * (max_intensity - min_intensity) + min_intensity;

      // Retrieve the actual intensity from the measurement data directly (already in the original scale)
      float actual_intensity = measurement_data[i + seq_length - 1][1];

      // Calculate deviations
      float absolute_deviation = abs(actual_intensity - predicted_intensity);
      
      // Calculate the percentage deviation between predicted and actual values
      float percent_deviation = (absolute_deviation / actual_intensity) * 100;

      // Anomaly detection based on fixed absolute and percentage thresholds
      float absolute_threshold = 500;  // Set a fixed absolute threshold for anomaly detection
      float percent_threshold = 20.0;  // Set a percentage threshold for anomaly detection


      // If either the absolute deviation or percent deviation exceeds these thresholds, we detect an anomaly.
      if (absolute_deviation > absolute_threshold || percent_deviation > percent_threshold) {
        Serial.print("Anomaly detected at timestamp: ");
        Serial.print(measurement_data[i + seq_length - 1][0]);
        Serial.print(" | Actual: ");
        Serial.print(actual_intensity);
        Serial.print(" | Predicted: ");
        Serial.print(predicted_intensity);
        Serial.print(" | Absolute deviation: ");
        Serial.print(absolute_deviation);
        Serial.print(" | Percent deviation: ");
        Serial.print(percent_deviation);
        Serial.println("%");
      } else {
        Serial.print("Normal at timestamp: ");
        Serial.print(measurement_data[i + seq_length - 1][0]);
        Serial.print(" | Actual: ");
        Serial.print(actual_intensity);
        Serial.print(" | Predicted: ");
        Serial.print(predicted_intensity);
        Serial.print(" | Absolute deviation: ");
        Serial.print(absolute_deviation);
        Serial.print(" | Percent deviation: ");
        Serial.print(percent_deviation);
        Serial.println("%");
      }
    }
  }
}
