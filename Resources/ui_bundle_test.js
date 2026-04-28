const React = require('react');

// Simple test UI component
const EchoGrainTestUI = () => {
    return React.createElement('View', {
        width: 1100,
        height: 680,
        backgroundColor: '#0a0a0a',
        justifyContent: 'center',
        alignItems: 'center'
    }, [
        React.createElement('Text', {
            key: 'title',
            fontSize: 36,
            color: '#ff00ff',
            textAlign: 'center',
            fontWeight: 'bold'
        }, 'ECHO GRAIN FX'),
        
        React.createElement('Text', {
            key: 'subtitle',
            fontSize: 18,
            color: '#00ffff',
            textAlign: 'center',
            marginTop: 20
        }, 'ETHEREAL GRANULAR SYNTHESIZER'),
        
        React.createElement('Text', {
            key: 'status',
            fontSize: 20,
            color: '#00ff00',
            textAlign: 'center',
            marginTop: 40,
            fontWeight: 'bold'
        }, '✓ REACT-JUCE UI LOADED'),
        
        React.createElement('Text', {
            key: 'version',
            fontSize: 14,
            color: '#ffaa00',
            textAlign: 'center',
            marginTop: 30
        }, 'Pure JavaScript Bundle • v1.0.0')
    ]);
};

// Global AppRegistry for react-juce
if (typeof global.__reactjuce_AppRegistry === 'undefined') {
    global.__reactjuce_AppRegistry = {};
}

global.__reactjuce_AppRegistry.registerComponent = function(name, component) {
    global.__reactjuce_AppRegistry[name] = component;
};

global.__reactjuce_AppRegistry.runApplication = function(name, props) {
    const Component = global.__reactjuce_AppRegistry[name];
    if (Component) {
        global.__reactjuce_RootComponent = Component;
    }
};

// Register the component
global.__reactjuce_AppRegistry.registerComponent("Main", () => EchoGrainTestUI);
global.__reactjuce_AppRegistry.runApplication("Main", { rootTag: 1 });
