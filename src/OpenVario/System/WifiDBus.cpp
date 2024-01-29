WiFiAgent::WiFiAgent(GDBusConnection *inputConnection, Poco::JSON::Object::Ptr credentials)
: connection(inputConnection), parameters(std::move(credentials)) {
    static const GDBusInterfaceVTable vtable = {
            .method_call  = handleMethodCall,
            .get_property = nullptr,
            .set_property = nullptr,
    };
    static GError* error = nullptr;

    objectId = g_dbus_connection_register_object(
            connection,
            WiFiAgent::ourAgentPath,
            WiFiAgent::introspectionWrapper->interfaces[0],
            &vtable,
            parameters.get(),
            nullptr,
            &error
    );
    if(objectId == 0 || error != nullptr)
        throw GlibException("Register WiFi agent", error);

    GVariant* agentPathVariant = g_variant_new("(o)", WiFiAgent::ourAgentPath);
    if(agentPathVariant == nullptr)
        throw std::runtime_error("Register WiFi agent: g_variant_new failed.");

    GVariant* result = g_dbus_connection_call_sync(connection, "net.connman", "/", "net.connman.Manager", 
"RegisterAgent", agentPathVariant, nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);
    if(result  == nullptr || error != nullptr)
        throw GlibException("Register WiFi agent", error);
}


//-----------------------------------------------------------------------------
void WiFiAgent::handleMethodCall(GDBusConnection *, const gchar *, 
    const gchar *, const gchar *, const gchar *method, 
    GVariant *methodParameters, GDBusMethodInvocation *invocation, gpointer userdata) {
    std::cout << "Method got called." << std::endl;
}
//-----------------------------------------------------------------------------
static inline class IntrospectionWrapper final {
    GDBusNodeInfo* introspection = nullptr;
    static constexpr auto introspectionXML =
        "<node>"
        "  <interface name='net.connman.Agent'>"
        "    <method name='RequestInput'>"
        "      <arg type='o' name='service' direction='in'/>"
        "      <arg type='a{sv}' name='fields' direction='in'/>"
        "      <arg type='a{sv}' name='fields' direction='out'/>"
        "    </method>"
        "    <method name='ReportError'>"
        "      <arg type='o' name='service' direction='in'/>"
        "      <arg type='s' name='error' direction='in'/>"
        "    </method>"
        "  </interface>"
        "</node>";

public:
    IntrospectionWrapper() {
        GError* error = nullptr;
        introspection = g_dbus_node_info_new_for_xml(introspectionXML, &error);
        if(introspection == nullptr || error != nullptr)
           std::cerr << GlibException("Agent introspection construction", error) << std::endl;
    }
    GDBusNodeInfo* operator->() { return introspection; };
    GDBusNodeInfo* get() { return introspection; }
} introspectionWrapper;
//-----------------------------------------------------------------------------
void DBusManipulator::connectToTheNetwork(GDBusProxy *network) {
    GError* error = nullptr;
    g_dbus_proxy_call_sync(network, "Connect", nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);
    if(error != nullptr)
        throw GlibException("Connect to the network", error);

    const auto state = variantGetValue(getNetworkProperty(network, "State"));
    if(state != "online" && state != "ready")
        throw std::runtime_error("Connect to the WiFi network: connection failed");

    std::cout << "Connected to the network successfully." << std::endl;
}
//-----------------------------------------------------------------------------
GDBusProxy* network = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START, nullptr, "net.connman", servicePath, "net.connman.Service", nullptr, &error);
if(network == nullptr || error != nullptr)
    throw GlibException("Get network by name", error);
//-----------------------------------------------------------------------------
