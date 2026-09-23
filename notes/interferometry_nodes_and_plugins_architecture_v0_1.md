**Interferometry Application --- Node & Plugin Architecture v0.1**

*Документ для обсуждения и утверждения архитектурных принципов*

  -----------------------------------------------------------------------
  **Статус:** Архитектурная концепция. Цель документа --- зафиксировать
  границы Core/Plugin, контракт pipeline-узлов и правила расширения.
  Конкретный C++ API пока не проектируется.
  -----------------------------------------------------------------------

  -----------------------------------------------------------------------

# Контекст

Приложение предназначено для обработки интерферограмм и получения
фазовых карт, волновых фронтов, карт отклонений поверхности и
результатов полиномиального анализа. Архитектура должна одновременно
поддерживать встроенные алгоритмы и внешние плагины, не создавая
различий в поведении Pipeline Engine.

- Базовые структуры данных должны быть независимы от MFC/Qt и пригодны
  для длительного хранения, воспроизводимого расчёта и плагинного
  расширения.

# 1. Цели архитектуры плагинов

- Разрешить замену практически любого вычислительного этапа pipeline
  внешним алгоритмом без изменения Core.

- Подключать внешние источники данных: камеры, интерферометры, видео,
  файлы и другие источники.

- Подключать алгоритмы фильтрации, трассировки полос, FFT, PSI, сшивки и
  другие алгоритмы, которые появятся позднее.

- Подключать импортёры и экспортёры внешних форматов.

- Сохранять единый канонический Data Model независимо от происхождения
  алгоритма.

- Обеспечить воспроизводимость: версия плагина, параметры и
  происхождение результата должны быть частью pipeline/provenance.

- Не связывать Plugin SDK с MFC или Qt.

# 2. Главный архитектурный принцип

  -----------------------------------------------------------------------
  **Принцип:** Built-in и external plugin являются для Pipeline Engine
  одним и тем же типом сущности --- Pipeline Node. Engine работает через
  контракты данных и портов, а не через конкретные алгоритмы.
  -----------------------------------------------------------------------

  -----------------------------------------------------------------------

> PIPELINE ENGINE\
> │\
> ┌───────┴───────┐\
> │ Node contract │\
> └───────┬───────┘\
> │\
> ┌─────────────┼─────────────┐\
> ▼ ▼ ▼\
> Built-in Node Plugin Node Plugin Node\
> │ │ │\
> └─────────────┼─────────────┘\
> ▼\
> Canonical Data Model

# 3. Граница Core и Plugin

Core отвечает за устойчивые семантические понятия и инфраструктуру.
Plugin отвечает за конкретную реализацию алгоритма, источника, приёмника
или расширения.

  -----------------------------------------------------------------------
               **Core**               **Plugin**
  ----------------------------------- -----------------------------------
  Document / Measurement / Workspace  Конкретный алгоритм

            Pipeline Engine           Node implementation

         Canonical Data Model         Алгоритмы фильтрации / FFT / PSI /
                                      tracing / stitching

  CoordinateFrame / Grid / Transform  Новая модель Transform, если она
              / Resampler             требуется

    Execution / cache / provenance    Import / Export

           Plugin Host / SDK          Hardware drivers / data sources

   Базовая визуальная инфраструктура  Необязательные custom editors /
                  UI                  dialogs / visualizations
  -----------------------------------------------------------------------

# 4. Канонические данные и порты

Каждый Node объявляет типизированные Input/Output ports. Передача данных
идёт через канонические типы Core, а не через void\*, сырые указатели на
матрицы или внутренние структуры конкретного алгоритма.

  -----------------------------------------------------------------------------
         **Тип**        **Назначение**                 **Типичный пример**
  --------------------- ------------------------------ ------------------------
      Interferogram     Интенсивность ИНТ +            Camera / File → Filter
                        acquisition metadata           

        PhaseMap        Фазовая карта с                FFT / PSI / LRV
                        Grid/Frame/Mask/Units          reconstruction

       SpectralSet      Спектральные представления     FFT → peak detection /
                        FFT; complex и real maps       visualization

     FringeSet / ЛРВ    Векторные центры полос         Tracing → numbering →
                                                       editing

    Mask / RegionSet    Геометрические области или     Aperture / spatial or
                        raster mask                    spectral filtering

       FiducialSet      Реперы и их координаты в       Registration /
                        известных СК                   calibration

   CoordinateTransform  Связь между двумя СК           G1 / G2 / G3

     CalibrationMap     КПФ в СКК                      Instrument correction

       SurfaceMap       КО: OPD / wavefront / surface  G3 → analysis
                        / deviation                    

      PolynomialFit     Базис + нормировка +           Zernike / Legendre /
                        коэффициенты + residual        Seidel
  -----------------------------------------------------------------------------

# 5. Особое уточнение для FFT: SpectralSet вместо чистого ComplexMap

FFT pipeline использует не только комплексные массивы. В одном расчёте
одновременно возникают комплексный спектр, действительные карты
амплитуды/модуля, логарифмированный спектр, маска спектральной области,
а после обратного FFT --- действительная фазовая карта. Поэтому базовый
результат FFT должен рассматриваться как спектральный набор, а не только
как ComplexMap.

> Interferogram\[spatial Grid\]\
> │\
> ▼\
> FFT\
> │\
> ├── Complex spectrum complex matrix\
> ├── Magnitude / log-mag real matrix\
> ├── Spectral mask spatially-defined in frequency domain\
> ├── Filtered spectrum complex matrix\
> └── Phase / derived maps real matrix

  -----------------------------------------------------------------------
  **Архитектурное следствие:** Спектральная маска является тем же классом
  концепции Region/Mask, что и апертурная маска, но действует на другой
  Grid и CoordinateFrame --- FrequencyGrid. Она может редактироваться
  интерактивно поверх визуализации амплитудного спектра.
  -----------------------------------------------------------------------

  -----------------------------------------------------------------------

# 6. Node Descriptor

Node должен декларативно описывать себя. Это позволяет Pipeline Engine и
Properties Pane работать с плагином без знания алгоритма.

> NodeDescriptor\
> id / version\
> name / category\
> input ports\[\]\
> output ports\[\]\
> parameter schema\
> execution mode\
> capabilities\
> resource requirements

### Примеры:

> FringeTracer\
> Input: Interferogram, Mask?\
> Output: FringeSet\
> \
> FFTPhase\
> Input: Interferogram, Mask?\
> Output: SpectralSet, PhaseMap?\
> \
> PSI\
> Input: Interferogram\[\] / phase-step metadata\
> Output: PhaseMap\
> \
> Stitcher\
> Input: PhaseMap\[\], Registration/FiducialSet\[\]\
> Output: SurfaceMap

# 7. Node является универсальной точкой расширения

Список расширений является открытым. В первой версии предусматриваются
следующие категории, но архитектура не должна содержать жёсткий перечень
категорий:

  -----------------------------------------------------------------------
     **Категория**    **Примеры**
  ------------------- ---------------------------------------------------
    Input / Source    camera, interferometer, video, file, stream

   Image Processing   filtering, background correction, denoising

         Phase        fringe tracing, FFT, spatial carrier, PSI
    Reconstruction    

   Fringe Processing  numbering, validation, interactive editing
                      assistance

     Calibration /    instrument calibration, registration, distortion,
       Geometry       CGH mapping

       Stitching      subaperture registration, blending, global
                      optimization

       Analysis       Zernike, Legendre, Seidel, statistics,
                      PSF/MTF/energy

      Simulation      synthetic interferograms, wavefront generation

    Import / Export   ESD, DAT, XYZ, FRN, MTR, INT, future formats

     UI Extension     custom editors, dialogs, visualizations, commands

     Future domain    shear interferometry and other methods
      extensions      
  -----------------------------------------------------------------------

# 8. Input / Source plugins

Источники являются плагинными. Они могут быть одноразовыми или
потоковыми.

> Camera / Interferometer plugin\
> ↓\
> Interferogram stream\
> \
> File Importer\
> ↓\
> Measurement / Interferogram / PhaseMap / FringeSet

### Рекомендуемые execution modes:

- Streaming --- видеокамера или live-интерферометр.

- One-shot --- загрузка отдельного файла или набора кадров.

- Batch --- импорт серии измерений.

# 9. Import / Export plugins

Import/Export используют тот же plugin infrastructure, но имеют
семантически специальные контракты. Внутренний Data Model не должен
зависеть от внешнего формата.

> External file\
> ↓\
> Format Probe\
> ↓\
> Format Adapter / Importer\
> ↓\
> Canonical Data Model\
> \
> Canonical Data Model\
> ↓\
> Exporter\
> ↓\
> External file

Importer возвращает не только данные, но и ImportReport: какие сущности
импортированы полностью, какие преобразованы, какие отсутствуют, какие
assumptions сделаны. Это особенно важно для обменных форматов, в которых
нет полного Measurement.

# 10. Расширяемость UI: принято решение A

Обязателен generic UI для параметров Node. Plugin может описывать
Parameter Schema, и Properties Pane строит редактор автоматически.

- Generic Properties Editor --- обязательный уровень.

- Custom Dialog / Editor --- необязательный уровень для сложных
  алгоритмов.

- Custom Visualization --- необязательный уровень, например собственный
  просмотр спектра.

- Custom commands / toolbar integration --- допускается через
  контролируемый UI extension API.

  -----------------------------------------------------------------------
  **Правило:** Наличие custom UI не должно быть обязательным условием
  запуска алгоритма. Node должен оставаться функционально доступным через
  generic API/UI.
  -----------------------------------------------------------------------

  -----------------------------------------------------------------------

# 11. Расширяемость данных: принято решение B

Plugin может регистрировать дополнительные data types, transform models
и serializers. Однако базовые Core types и их семантика остаются
стабильными.

> Core types\
> +\
> Plugin-registered types\
> \
> Plugin may provide:\
> DataType\
> Serializer\
> NodeFactory\
> CoordinateTransformModel\
> UI extension

Новый тип данных не должен незаметно преобразовываться в другой тип.
Переход должен быть явным Node/adapter и отображаться в provenance.

# 12. CoordinateTransform и Resampler

Transform и Resampler являются частью Core. Plugin может поставлять
новую математическую модель Transform, но обычно не должен дублировать
базовый механизм resampling.

> CoordinateTransform\
> maps coordinates\
> \
> Resampler\
> maps sampled fields from source Grid to target Grid\
> \
> Plugin-provided CGH mapping\
> → CoordinateTransform\
> → Core Resampler

Это сохраняет прозрачность метрологического pipeline: изменения СК/Grid
являются явными операциями и не прячутся внутри FFT, tracing или другого
алгоритма.

# 13. Immutable inputs и владение памятью

Input data для Node рассматриваются как immutable. Node создаёт output,
а Core управляет временем жизни и cache. Для больших массивов должен
существовать reference-counted/shared DataBuffer, допускающий zero-copy
и последующую работу с GPU/camera buffers.

> Input Map ──const──→ Node\
> │\
> └──→ Output Map\
> \
> No in-place modification of upstream canonical data.

  -----------------------------------------------------------------------
  **Архитектурное следствие:** Immutable inputs упрощают cache, parallel
  execution, provenance и воспроизводимость результатов.
  -----------------------------------------------------------------------

  -----------------------------------------------------------------------

# 14. Plugin SDK и ABI

Официальная DLL-граница должна быть стабильной. Рекомендуемая схема ---
C ABI с opaque handles; поверх неё можно предоставить удобный C++ SDK
wrapper.

> Application Core\
> │\
> stable C ABI\
> │\
> Plugin DLL\
> │\
> C++ SDK wrapper\
> │\
> Plugin implementation

- Plugin SDK не зависит от MFC.

- Plugin SDK не зависит от Qt.

- В ABI не следует использовать STL containers, std::string,
  std::shared_ptr и иные C++ ABI-sensitive сущности.

- Внутри plugin можно свободно использовать современный C++.

# 15. Plugin Context

Plugin получает ограниченный контекст выполнения, а не прямой доступ к
внутренностям Document/UI/Pipeline Engine.

> PluginContext\
> Data access\
> Parameters\
> Logger\
> Progress\
> Cancellation\
> Resource access\
> Coordinate services

Это снижает связанность и позволяет со временем менять UI, storage и
внутреннюю реализацию Core без изменения plugin semantics.

# 16. Execution modes

  ------------------------------------------------------------------------
    **Mode**    **Типичные Node**         **Особенности**
  ------------- ------------------------- --------------------------------
    One-shot    FFT, phase                один входной набор → результат
                reconstruction, export    

      Batch     stitching, series         много наборов данных
                processing, report        
                generation                

    Streaming   camera/interferometer     последовательность кадров
                input                     

   Interactive  FringeEditor, spectral    контролируемое пользователем
                mask editor               изменение данных

     Hybrid     live acquisition + online stream + incremental processing
                phase                     
  ------------------------------------------------------------------------

# 17. Interactive masks и FFT

Архитектура должна одинаково поддерживать маски в разных областях
определения. Пространственная маска действует на ИНТ/PhaseMap Grid;
спектральная маска действует на FrequencyGrid.

> Spatial domain Frequency domain\
> \
> Interferogram \[Image Grid\] Spectrum \[Frequency Grid\]\
> │ │\
> └── Spatial Mask └── Spectral Mask\
> │ │\
> └──────── processing / inverse FFT ────┘

Спектральная маска может быть геометрическим объектом (например,
ellipse/polygon/region), интерактивно размещённым поверх карты \|FFT\|
или log\|FFT\|. Маска должна сохраняться как объект с собственной
областью определения, а не как безымянная часть конкретной реализации
FFT.

# 18. ЛРВ / FringeSet как plugin boundary

ЛРВ и центры полос являются единым типом данных. Их геометрия
определяется последовательностью точек/кривых; полосы могут
соответствовать максимумам или минимумам интенсивности. Это позволяет
разделить алгоритмы автоматической трассировки и последующее ручное
редактирование.

> Interferogram\
> ↓\
> FringeTracer Plugin\
> ↓\
> FringeSet / ЛРВ\
> ↓\
> Numbering / Validation\
> ↓\
> Interactive Fringe Editor\
> ↓\
> Phase Reconstruction

# 19. Пример заменяемого pipeline

> CameraPlugin\
> ↓\
> FilterPlugin A\
> ↓\
> Geometry / Calibration\
> ↓\
> ┌──────────────┬──────────────┬──────────────┐\
> │ FringeTracer │ FFTPhase │ PSI │\
> └──────────────┴──────────────┴──────────────┘\
> ↓\
> PhaseMap\
> ↓\
> Instrument Calibration \[СКК\]\
> ↓\
> Alignment Correction\
> ↓\
> CGH Mapping\
> ↓\
> SurfaceMap\
> ↓\
> Polynomial Analysis

Любой из алгоритмов в данном pipeline может быть встроенным либо
предоставлен plugin --- без изменения downstream-контракта, если
сохраняется тот же output type/semantics.

# 20. Plugin versioning и воспроизводимость

Сохранённый pipeline должен фиксировать идентичность алгоритма и его
параметры.

> Node instance\
> node type ID\
> plugin ID\
> plugin version\
> node/schema version\
> parameters\
> input references\
> output/provenance

  -----------------------------------------------------------------------
  **Правило восстановления:** Отсутствие plugin не должно разрушать
  сохранённый Measurement/Pipeline. Node получает состояние
  unresolved/missing plugin; документ можно открыть, просматривать и
  восстанавливать после установки требуемой версии.
  -----------------------------------------------------------------------

  -----------------------------------------------------------------------

# 21. Provenance

Каждый производный результат должен сохранять происхождение: inputs,
operation, parameters, versions и связанные геометрические/калибровочные
сущности.

> PhaseMap #42\
> source = FringeSet #17\
> algorithm = Plugin:FringeTracer 2.3\
> parameters = {\...}\
> geometry = T_OS\<-I #5\
> calibration = KPF #12\
> alignment = Model #4

# 22. Что не должно становиться Plugin API

- Базовая семантика CoordinateFrame/Grid/Map/Mask не должна подменяться
  конкретным plugin.

- Plugin не должен напрямую менять upstream data.

- Plugin не должен зависеть от конкретной UI framework.

- Plugin не должен скрывать существенное Coordinate/Resampling
  преобразование внутри другого Node.

- Pipeline Engine не должен содержать специальные ветви для отдельных
  производителей алгоритмов или файлов.

# 23. Итоговая модель

> APPLICATION CORE\
> ┌─────────────────────────────────────────────────────────┐\
> │ Canonical Data Model │\
> │ Coordinate / Grid / Transform / Resampler │\
> │ Pipeline Engine / Cache / Provenance │\
> │ Plugin Host │\
> └──────────────────────────────┬──────────────────────────┘\
> │ stable Plugin SDK\
> ┌────────────────┼────────────────┐\
> ▼ ▼ ▼\
> Sources Processing Sinks\
> camera / files filter / FFT import/export\
> interferometers PSI / tracing reports\
> video / streams stitching future formats\
> │\
> any pipeline node\
> │\
> Built-in == Plugin implementation

# 24. Решения, предлагаемые к утверждению

1.  Node является основной единицей расширения pipeline.

2.  Built-in и external nodes используют единый типизированный контракт
    ports/data.

3.  Core владеет canonical data types и их семантикой.

4.  Plugin может регистрировать новые data types, serializers, transform
    models и nodes.

5.  Generic Properties UI обязателен; custom UI является дополнительным.

6.  CoordinateTransform и Resampler разделены; существенные
    геометрические преобразования должны быть явными в pipeline.

7.  Input data immutable; Core управляет ownership/cache; допускается
    zero-copy DataBuffer.

8.  Plugin SDK не зависит от MFC/Qt; DLL boundary --- стабильный C ABI с
    C++ wrapper.

9.  Поддерживаются One-shot, Batch, Streaming и Interactive execution
    modes.

10. Import/Export и hardware input реализуются через общий plugin
    infrastructure.

11. Pipeline сохраняет plugin identity, version, schema и provenance;
    missing plugin не разрушает документ.

12. SpectralSet/FFT data model допускает одновременно complex и real
    maps и отдельную Spectral Mask на FrequencyGrid.

13. ЛРВ/FringeSet является canonical векторным представлением центров
    полос и может проходить через tracing, numbering и interactive
    editing.

# 25. Что сознательно оставлено для следующего этапа

- Конкретные C/C++ ABI-функции, opaque handles и DataBuffer.

- Discovery/loading/unloading DLL, sandboxing и compatibility policy.

- Serialization format для plugin descriptors и pipeline schema.

- Полный UI extension API, command routing и threading/execution
  scheduler.

# Заключение

Архитектура делает pipeline независимым от конкретных алгоритмов и
производителей, сохраняя единую метрологическую модель данных. Основная
единица расширения --- типизированный Node; основная граница
совместимости --- canonical Data Contract. Это позволяет наращивать
приложение плагинами для оборудования, FFT/PSI/tracing, stitching и
внешних форматов без изменения фундаментальной модели измерения.
