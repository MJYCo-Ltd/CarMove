#ifndef FUELUNLOADINGDATALOADER_H
#define FUELUNLOADINGDATALOADER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QQmlEngine>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class FuelUnloadingDataLoader : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(QVariantList vehicles READ vehicles NOTIFY vehiclesChanged)
    Q_PROPERTY(bool isLoaded READ isLoaded NOTIFY isLoadedChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    
public:
    explicit FuelUnloadingDataLoader(QObject *parent = nullptr);
    
    // Property getters
    QVariantList vehicles() const { return m_vehicles; }
    bool isLoaded() const { return m_isLoaded; }
    QString errorMessage() const { return m_errorMessage; }
    
    // Invokable methods for QML
    Q_INVOKABLE bool loadFromFile(const QString& filePath);
    Q_INVOKABLE bool loadFromResource(const QString& resourcePath);
    Q_INVOKABLE QVariantList getAllRecords();
    Q_INVOKABLE QVariantMap getStatistics();
    Q_INVOKABLE void clearData();
    
signals:
    void vehiclesChanged();
    void isLoadedChanged();
    void errorMessageChanged();
    void dataLoaded(bool success, const QString& message);
    
private:
    bool parseJsonData(const QJsonDocument& doc);
    QVariantMap recordToVariant(const QJsonObject& record);
    void setError(const QString& error);
    void clearError();
    
    QVariantList m_vehicles;
    bool m_isLoaded;
    QString m_errorMessage;
};

#endif // FUELUNLOADINGDATALOADER_H