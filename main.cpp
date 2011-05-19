//  The contents of this file are subject to the Mozilla Public License
//  Version 1.1 (the "License"); you may not use this file except in
//  compliance with the License. You may obtain a copy of the License at
//  http://www.mozilla.org/MPL/
//
//  Software distributed under the License is distributed on an "AS IS"
//  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
//  License for the specific language governing rights and limitations
//  under the License.
//
//  The Original Code is "Qt Project File Generator".
//
//  The Initial Developer of the Original Code is Darren R. Starr.
//
//  Contributor(s):
//           Darren R. Starr <submux@gmail.com>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileInfoList>
#include <QtCore/QRegExp>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>

/// A class which recursively traverses a directory tree to locate files that
///   should be added to a project.
class ProjectItems
{
public:
  /// Constructor
  ProjectItems()
  {
    setDefaultExpressions();
  }

  /// Sets the default regular expressions for searching for files to include
  void setDefaultExpressions()
  {
    m_headerExpression = QRegExp("^.*\\.(h|hpp)$");
    m_sourceExpression =  QRegExp("^.*\\.(c|cpp)$");
  }

  /// Scans the given directory recursively for files which match the configured
  ///  file name patterns for each project component type.
  ///
  /// \param path the path to search
  /// \param clear specifies whether the found objects list should be cleared first.
  void scan(const QDir &path, bool clear=true)
  {
    if(clear)
    {
      m_headerFiles.clear();
      m_sourceFiles.clear();
    }

    QFileInfoList infoList = path.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    foreach(QFileInfo fi, infoList)
    {
      qDebug() << fi.absoluteFilePath();
      if(fi.isDir())
      {
        scan(QDir(fi.absoluteFilePath()), false);
      }
      else if(fi.fileName().indexOf(m_headerExpression, 0, Qt::CaseInsensitive) == 0)
      {
        m_headerFiles.append(fi.absoluteFilePath());
      }
      else if(fi.fileName().indexOf(m_sourceExpression, 0, Qt::CaseInsensitive) == 0)
      {
        m_sourceFiles.append(fi.absoluteFilePath());
      }
    }
    m_headerFiles.sort();
    m_sourceFiles.sort();
  }

  /// Returns the list of header files located during the scan.
  const QStringList &headerFiles() const
  {
    return m_headerFiles;
  }

  /// Returns the list of source files located during the scan.
  const QStringList &sourceFiles() const
  {
    return m_sourceFiles;
  }

private:
  QStringList m_headerFiles;
  QStringList m_sourceFiles;

  QRegExp m_headerExpression;
  QRegExp m_sourceExpression;
};

/// Parses a given path for files and adds them to a project of the given name.
///
/// This produces a very simplistic Qt QMake .pro project that can be used to easily
/// load a full path worth of source code into Qt Creator.
///
/// With a little bit of work, it would be possible to make something more advanced
/// which could actually be used for building projects within Qt Creator.
class Project
{
public:
  /// Constructor
  Project()
  {
  }

  /// Sets the name of the output file for the .pro
  void setOutputFileName(const QString &fileName)
  {
    m_outputFileName = fileName;
  }

  /// Enumarates all the files for the projects within the given path
  ///
  /// \param projectRoot the root path to search for files
  /// \return always true for now.
  bool enumerateItems(const QDir &projectRoot)
  {
    m_projectRoot = projectRoot;
    m_project.scan(projectRoot);
    return true;
  }

  /// Creates the project file which should be able to be openned by Qt Creator.
  ///
  /// \return true if the file was generated, false if the output file could not be
  ///              opened.
  bool generateProject()
  {
    QFile outputFile(m_outputFileName);
    if(!outputFile.open(QIODevice::WriteOnly))
    {
      qCritical() << "Failed to open the specified output file for writing";
      qCritical() << m_outputFileName;
      return false;
    }

    QTextStream out(&outputFile);

    out << "TEMPLATE = app" << endl;
    out << endl;
    out << "HEADERS =";
    foreach(const QString &headerFile, m_project.headerFiles())
    {
      out << " \\" << endl << "  " << m_projectRoot.relativeFilePath(headerFile);
    }
    out << endl;
    out << endl;
    out << "SOURCES =";
    foreach(const QString &sourceFile, m_project.sourceFiles())
    {
      out << " \\" << endl << "  " << m_projectRoot.relativeFilePath(sourceFile);
    }
    out << endl;

    return true;
  }

private:
  ProjectItems m_project;
  QString m_outputFileName;
  QDir m_projectRoot;
};

/// Represents a command line option made up of a name, a value or a name and a value
class Option
{
public:
  /// Constructor
  ///
  /// \param name the name of the option.
  /// \param value the value of the option.
  Option(const QString &name, const QString &value) :
    m_name(name),
    m_value(value)
  {
  }

  /// Constructor
  ///
  /// \param name the name of the option.
  Option(const QString &name) :
    m_name(name)
  {
  }

  /// Returns the name of the option
  const QString &name() const
  {
    return m_name;
  }

  /// Returns the value of the option.
  const QString &value() const
  {
    return m_value;
  }

private:
  QString m_name;
  QString m_value;
};

/// Provides a class for parsing command line options in a relatively simple format.
///
/// Command line options are passed during startup of the application in the standard
/// C format for argc and argv. They are then parsed out based on the following criteria.
/// \li --name value
///     where name is the name of the option and value is a string which does not start
///     with the two characters "--"
/// \li --name
///     where the name if the name of the option. No option is store for this option, but
///     the option can be seen as being a flag.
/// \li value
///     an option that doesn't need a name such as when passing the source file name to a
///     compiler.
class Options
{
public:
  /// Constructor
  ///
  /// \param argc the number of arguments passed in argv.
  /// \param argv the arguments passed on the command line. (the first is ignored)
  Options(int argc, char **argv)
  {
    QStringList args;
    for(int i=0; i<argc; i++)
      args.append(argv[i]);

    int i=1;
    while(i < args.count())
    {
      if(args[i].startsWith("--"))
      {
        if(i < (args.count() - 1))
        {
          if(args[i + 1].startsWith("--"))      // current starts with "--", next also starts with "--"
          {
            addFlag(args[i]);
            i++;
            continue;
          }
          else                                  // current starts with "--", next does not start with "--"
          {
            addOption(args[i], args[i+1]);
            i += 2;
            continue;
          }
        }
        else                                    // last argument starts with "--"
        {
          addFlag(args[i]);
          i++;
          continue;
        }
      }

      // Current does not start with "--" so is just a stand along value
      addValue(args[i]);
      i++;
    }
  }

  /// Destructor
  ~Options()
  {
    while(m_options.count())
      delete m_options.takeFirst();
  }

  /// Returns a list of all the options
  const QList<Option *> &options() const
  {
    return m_options;
  }

  /// Returns a list of all the options which match the given name
  ///
  /// \param name the name to match for the option. Leave this blank for values
  ///             that don't have a name.
  /// \return the list of options found.
  QList<const Option *>options(const QString &name) const
  {
    QList<const Option *>result;

    foreach(const Option *option, m_options)
    {
      if(option->name() == name)
        result.append(option);
    }

    return result;
  }

  /// Returns whether any options of the given name are present
  ///
  /// \param name the name to match or blank for values without names.
  /// \return true if an option with the given name is found.
  bool present(const QString &name) const
  {
    foreach(const Option *option, m_options)
    {
      if(option->name() == name)
        return true;
    }
    return false;
  }

  /// Returns the value of an option with the given name.
  ///
  /// \param name the name of the option or a blank string for when there
  ///             is no name.
  /// \param index the zero based index of the occurance of the name. So
  ///             if the second instance of --bob is wanted, then this value
  ///             should be 1. Additionally, if this value is -1, then the last
  ///             item is returned instead.
  /// \return either the value of the requested option or a null QString if
  ///             there is no option at the given index. (Check with QString.isNull())
  QString value(const QString &name, int index=0) const
  {
    QList<const Option *>values = options(name);
    if(!values.count() && (index >= values.count() || index < 0))
      return QString::Null();

    if(index == -1)
      return values.last()->value();

    return values[index]->value();
  }

protected:
  void addFlag(const QString &name)
  {
    m_options.append(new Option(name));
  }

  void addValue(const QString &value)
  {
    m_options.append(new Option("", value));
  }

  void addOption(const QString &name, const QString &value)
  {
    m_options.append(new Option(name, value));
  }

private:
  QList<Option *> m_options;
};


int main(int argc, char **argv)
{
    Options options(argc, argv);

    QString outputFileName = options.value("", -1);
    if(outputFileName.isNull())
    {
      qCritical() << "Output file name is not present on the command line";
      return -1;
    }

    // Get the absolute path
    QFileInfo fi(outputFileName);
    outputFileName = fi.absoluteFilePath();

    // Set the project root
    QDir projectRoot(fi.absoluteDir());
    if(!projectRoot.exists())
    {
      qCritical() << "The project path defined does not exist";
      qCritical() << projectRoot.absolutePath();
      return -1;
    }

    Project project;
    project.setOutputFileName(outputFileName);
    if(!project.enumerateItems(projectRoot))
    {
      qCritical() << "Failed to enumerate any items in the given path";
      return -1;
    }

    if(!project.generateProject())
    {
      return -1;
    }

    return 0;
}
