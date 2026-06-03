slackybara0 by captnchopsticks

v1 current functionality:
- wifi:
  - autoconnect to saved ssid & password
  - display nearby ssid list, with options for open and closed networks
  - manual connection if autoconnect fails
- gemini API:
  - leverages Google Gemini's REST API: https://ai.google.dev/api
  - uses DynamicJSONDocument approach from the ArduinoJSON lib to prepare JSON payload (better organization than the hard-coded approach)
  - prompts user to ask question, prints out Gemini response (or error code) to user
  - multi-turn response functionality, meaning chat history between user and Gemini is captured and appended into the JSON payload as chat continues.
  - full functionality to adjust model parameters on the fly, including:
    - model version (gemini flash, gemini flash lite, etc.)
    - max token output
    - model thinking budget, include thoughts
    - model temperature
  - uses the "latest" alias to hopefully prevent api service disruption as older models become depricated (EX: gemini-flash-latest)
  - uses sliding-window pruning for chat history to save on memory
- calculator:
  - leverages tinyexpr lib to parse user inputs (apply PEMDAS) and return the answer
  - currently very basic, no where near to cloning all the functions of the TI-30XS
- state machine:
  - type in EXIT during any point to exit back to the menu state
  - currently three "apps": menu, gemini, calculator, and settings
