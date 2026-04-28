var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));

// js/index.tsx
var import_react = __toESM(require("react"));
var import_react_juce = require("react-juce");
var EchoGrainFX = () => {
  const [grainSize, setGrainSize] = (0, import_react.useState)(50);
  const [grainDensity, setGrainDensity] = (0, import_react.useState)(30);
  const [grainPitch, setGrainPitch] = (0, import_react.useState)(0);
  const [delayTime, setDelayTime] = (0, import_react.useState)(250);
  const [feedback, setFeedback] = (0, import_react.useState)(40);
  const [wetDry, setWetDry] = (0, import_react.useState)(50);
  return import_react.default.createElement(import_react_juce.View, {
    style: {
      width: 1100,
      height: 680,
      backgroundColor: "#0b0d12",
      flexDirection: "column",
      padding: 20
    }
  }, [
    // Header
    import_react.default.createElement(import_react_juce.View, {
      key: "header",
      style: {
        width: "100%",
        height: 80,
        backgroundColor: "#1a1f2e",
        borderRadius: 8,
        alignItems: "center",
        justifyContent: "center",
        marginBottom: 20,
        borderWidth: 1,
        borderColor: "#2d3748"
      }
    }, [
      import_react.default.createElement(import_react_juce.Text, {
        key: "title",
        style: {
          color: "#4EF0FF",
          fontSize: 28,
          fontWeight: "bold",
          textAlign: "center"
        }
      }, "ECHO GRAIN FX"),
      import_react.default.createElement(import_react_juce.Text, {
        key: "subtitle",
        style: {
          color: "#7C3AED",
          fontSize: 14,
          textAlign: "center",
          marginTop: 5
        }
      }, "ETHEREAL GRANULAR SYNTHESIZER")
    ]),
    // Main Controls
    import_react.default.createElement(import_react_juce.View, {
      key: "main-controls",
      style: {
        flexDirection: "row",
        width: "100%",
        height: 300,
        justifyContent: "space-between"
      }
    }, [
      // Granular Section
      import_react.default.createElement(import_react_juce.View, {
        key: "granular-section",
        style: {
          width: 320,
          backgroundColor: "#1a1f2e",
          borderRadius: 8,
          padding: 20,
          borderWidth: 1,
          borderColor: "#2d3748"
        }
      }, [
        import_react.default.createElement(import_react_juce.Text, {
          key: "granular-title",
          style: {
            color: "#4EF0FF",
            fontSize: 16,
            fontWeight: "bold",
            marginBottom: 20,
            textAlign: "center"
          }
        }, "GRANULAR ENGINE"),
        // Grain Size
        import_react.default.createElement(import_react_juce.View, {
          key: "grain-size-control",
          style: { marginBottom: 25 }
        }, [
          import_react.default.createElement(import_react_juce.Text, {
            key: "grain-size-label",
            style: {
              color: "#A0AEC0",
              fontSize: 12,
              marginBottom: 8
            }
          }, `GRAIN SIZE: ${grainSize}ms`),
          import_react.default.createElement(import_react_juce.Slider, {
            key: "grain-size-slider",
            style: {
              width: "100%",
              height: 30
            },
            min: 1,
            max: 200,
            value: grainSize,
            onValueChange: setGrainSize
          })
        ]),
        // Grain Density
        import_react.default.createElement(import_react_juce.View, {
          key: "grain-density-control",
          style: { marginBottom: 25 }
        }, [
          import_react.default.createElement(import_react_juce.Text, {
            key: "grain-density-label",
            style: {
              color: "#A0AEC0",
              fontSize: 12,
              marginBottom: 8
            }
          }, `DENSITY: ${grainDensity}%`),
          import_react.default.createElement(import_react_juce.Slider, {
            key: "grain-density-slider",
            style: {
              width: "100%",
              height: 30
            },
            min: 0,
            max: 100,
            value: grainDensity,
            onValueChange: setGrainDensity
          })
        ]),
        // Grain Pitch
        import_react.default.createElement(import_react_juce.View, {
          key: "grain-pitch-control",
          style: { marginBottom: 15 }
        }, [
          import_react.default.createElement(import_react_juce.Text, {
            key: "grain-pitch-label",
            style: {
              color: "#A0AEC0",
              fontSize: 12,
              marginBottom: 8
            }
          }, `PITCH: ${grainPitch > 0 ? "+" : ""}${grainPitch} ST`),
          import_react.default.createElement(import_react_juce.Slider, {
            key: "grain-pitch-slider",
            style: {
              width: "100%",
              height: 30
            },
            min: -24,
            max: 24,
            value: grainPitch,
            onValueChange: setGrainPitch
          })
        ])
      ]),
      // Echo Section
      import_react.default.createElement(import_react_juce.View, {
        key: "echo-section",
        style: {
          width: 320,
          backgroundColor: "#1a1f2e",
          borderRadius: 8,
          padding: 20,
          borderWidth: 1,
          borderColor: "#2d3748"
        }
      }, [
        import_react.default.createElement(import_react_juce.Text, {
          key: "echo-title",
          style: {
            color: "#7C3AED",
            fontSize: 16,
            fontWeight: "bold",
            marginBottom: 20,
            textAlign: "center"
          }
        }, "ECHO ENGINE"),
        // Delay Time
        import_react.default.createElement(import_react_juce.View, {
          key: "delay-time-control",
          style: { marginBottom: 25 }
        }, [
          import_react.default.createElement(import_react_juce.Text, {
            key: "delay-time-label",
            style: {
              color: "#A0AEC0",
              fontSize: 12,
              marginBottom: 8
            }
          }, `DELAY: ${delayTime}ms`),
          import_react.default.createElement(import_react_juce.Slider, {
            key: "delay-time-slider",
            style: {
              width: "100%",
              height: 30
            },
            min: 10,
            max: 2e3,
            value: delayTime,
            onValueChange: setDelayTime
          })
        ]),
        // Feedback
        import_react.default.createElement(import_react_juce.View, {
          key: "feedback-control",
          style: { marginBottom: 25 }
        }, [
          import_react.default.createElement(import_react_juce.Text, {
            key: "feedback-label",
            style: {
              color: "#A0AEC0",
              fontSize: 12,
              marginBottom: 8
            }
          }, `FEEDBACK: ${feedback}%`),
          import_react.default.createElement(import_react_juce.Slider, {
            key: "feedback-slider",
            style: {
              width: "100%",
              height: 30
            },
            min: 0,
            max: 95,
            value: feedback,
            onValueChange: setFeedback
          })
        ])
      ]),
      // Master Section
      import_react.default.createElement(import_react_juce.View, {
        key: "master-section",
        style: {
          width: 320,
          backgroundColor: "#1a1f2e",
          borderRadius: 8,
          padding: 20,
          borderWidth: 1,
          borderColor: "#2d3748"
        }
      }, [
        import_react.default.createElement(import_react_juce.Text, {
          key: "master-title",
          style: {
            color: "#10B981",
            fontSize: 16,
            fontWeight: "bold",
            marginBottom: 20,
            textAlign: "center"
          }
        }, "MASTER"),
        // Wet/Dry Mix
        import_react.default.createElement(import_react_juce.View, {
          key: "wetdry-control",
          style: { marginBottom: 25 }
        }, [
          import_react.default.createElement(import_react_juce.Text, {
            key: "wetdry-label",
            style: {
              color: "#A0AEC0",
              fontSize: 12,
              marginBottom: 8
            }
          }, `WET/DRY: ${wetDry}%`),
          import_react.default.createElement(import_react_juce.Slider, {
            key: "wetdry-slider",
            style: {
              width: "100%",
              height: 30
            },
            min: 0,
            max: 100,
            value: wetDry,
            onValueChange: setWetDry
          })
        ])
      ])
    ]),
    // Status Footer
    import_react.default.createElement(import_react_juce.View, {
      key: "footer",
      style: {
        width: "100%",
        height: 60,
        backgroundColor: "#1a1f2e",
        borderRadius: 8,
        alignItems: "center",
        justifyContent: "center",
        marginTop: 20,
        borderWidth: 1,
        borderColor: "#2d3748"
      }
    }, [
      import_react.default.createElement(import_react_juce.Text, {
        key: "status",
        style: {
          color: "#10B981",
          fontSize: 14,
          fontWeight: "bold"
        }
      }, "\u25CF REACT-JUCE UI ACTIVE"),
      import_react.default.createElement(import_react_juce.Text, {
        key: "version",
        style: {
          color: "#6B7280",
          fontSize: 11,
          marginTop: 5
        }
      }, "EchoGrainFX v1.0.0 \u2022 SolarBumper")
    ])
  ]);
};
import_react_juce.AppRegistry.registerComponent("Main", () => EchoGrainFX);
import_react_juce.AppRegistry.runApplication("Main", { rootTag: 1 });
