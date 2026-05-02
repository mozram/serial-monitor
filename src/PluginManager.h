#ifndef PLUGINMANAGER_H_DEF
#define PLUGINMANAGER_H_DEF

#include <QStringList>
#include <QMap>

// The conflict: Qt defines 'slots' as a macro, but Python.h uses it as a member name.
#pragma push_macro("slots")
#undef slots
#include <pybind11/embed.h>
#pragma pop_macro("slots")

namespace py = pybind11;

class PluginManager {
public:
    static PluginManager& instance();
    
    void refreshPlugins();
    QStringList pluginNames() const;
    QString decode(const QString& pluginName, const QByteArray& data);

private:
    PluginManager();
    ~PluginManager();
    
    // Using a pointer to avoid visibility issues in the header with embedded types
    struct Impl;
    Impl* m_impl;
};

#endif
