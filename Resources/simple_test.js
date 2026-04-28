// Simple React-JUCE test interface
const React = require('react');

function TestInterface(props) {
    return React.createElement('View', {
        width: 1100,
        height: 680,
        backgroundColor: '#0a0a0a',
        justifyContent: 'center',
        alignItems: 'center'
    }, [
        React.createElement('Text', {
            key: 'main-title',
            fontSize: 32,
            color: '#ff00ff',
            textAlign: 'center'
        }, 'ECHO GRAIN FX'),
        
        React.createElement('Text', {
            key: 'subtitle',
            fontSize: 16,
            color: '#00ffff', 
            textAlign: 'center',
            marginTop: 15
        }, 'React-JUCE Interface Test'),
        
        React.createElement('Text', {
            key: 'status',
            fontSize: 18,
            color: '#00ff00',
            textAlign: 'center',
            marginTop: 30
        }, 'SUCCESS: React Component Loaded!')
    ]);
}

// Standard AppRegistry pattern for react-juce
const AppRegistry = {
    registerComponent: function(name, component) {
        global.__reactjuce_AppRegistry = global.__reactjuce_AppRegistry || {};
        global.__reactjuce_AppRegistry[name] = component;
    },
    
    runApplication: function(name, props) {
        const Component = global.__reactjuce_AppRegistry[name];
        if (Component) {
            global.__reactjuce_RootComponent = Component;
        }
    }
};

AppRegistry.registerComponent("Main", () => TestInterface);
AppRegistry.runApplication("Main", { rootTag: 1 });
