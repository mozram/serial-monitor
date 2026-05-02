#include "PluginManager.h"
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

struct PluginManager::Impl {
    py::scoped_interpreter guard;
    QMap<QString, py::object> plugins;
};

PluginManager& PluginManager::instance() {
    static PluginManager inst;
    return inst;
}

PluginManager::PluginManager() : m_impl(new Impl()) {
    refreshPlugins();
}

PluginManager::~PluginManager() {
    delete m_impl;
}

void PluginManager::refreshPlugins() {
    m_impl->plugins.clear();
    
    QDir pluginsDir(QCoreApplication::applicationDirPath() + "/plugins");
    if (!pluginsDir.exists()) {
        pluginsDir.mkpath(".");
    }

    try {
        py::module_ sys = py::module_::import("sys");
        sys.attr("path").attr("append")(pluginsDir.absolutePath().toStdString());

        QStringList filters;
        filters << "*.py";
        for (const QString& fileName : pluginsDir.entryList(filters, QDir::Files)) {
            QString stem = fileName.section('.', 0, 0);
            try {
                py::module_ plugin = py::module_::import(stem.toStdString().c_str());
                if (py::hasattr(plugin, "decode_serial")) {
                    m_impl->plugins[stem] = plugin.attr("decode_serial");
                    qDebug() << "Loaded plugin:" << stem;
                }
            } catch (const py::error_already_set& e) {
                qWarning() << "Failed to load plugin" << fileName << ":" << e.what();
            }
        }
    } catch (const py::error_already_set& e) {
        qWarning() << "Python error during refresh:" << e.what();
    }
}

QStringList PluginManager::pluginNames() const {
    return m_impl->plugins.keys();
}

QString PluginManager::decode(const QString& pluginName, const QByteArray& data) {
    if (!m_impl->plugins.contains(pluginName)) return QString::fromUtf8(data);
    
    try {
        py::object result = m_impl->plugins[pluginName](py::bytes(data.data(), data.size()));
        return QString::fromStdString(py::str(result));
    } catch (const py::error_already_set& e) {
        return QString("ERROR [Plugin %1]: %2").arg(pluginName, e.what());
    }
}
