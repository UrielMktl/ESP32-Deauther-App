import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'models.dart';
import 'controller.dart';
import 'esp_api.dart';
import 'ble_control.dart';

const _rojo = Color(0xFFEF2B3C);

void main() => runApp(MaterialApp(
      title: 'ESP32-Deauther',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: _rojo),
        useMaterial3: true,
      ),
      home: const HomePage(),
    ));

class HomePage extends StatefulWidget {
  const HomePage({super.key});
  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  String _mode = 'wifi';

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('ESP32-Deauther'),
        backgroundColor: _rojo,
        foregroundColor: Colors.white,
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          const Card(
            child: ListTile(
              leading: Icon(Icons.router, color: _rojo, size: 36),
              title: Text('Controla tu ESP32',
                  style: TextStyle(fontWeight: FontWeight.bold)),
              subtitle: Text('Por WiFi o por Bluetooth.\nSolo en tus propias redes.'),
            ),
          ),
          const SizedBox(height: 12),
          SegmentedButton<String>(
            segments: const [
              ButtonSegment(
                  value: 'wifi',
                  label: Text('WiFi'),
                  icon: Icon(Icons.wifi)),
              ButtonSegment(
                  value: 'ble',
                  label: Text('Bluetooth'),
                  icon: Icon(Icons.bluetooth)),
            ],
            selected: {_mode},
            onSelectionChanged: (s) => setState(() => _mode = s.first),
          ),
          const SizedBox(height: 12),
          _mode == 'wifi' ? const WifiLoginForm() : const BleConnectScreen(),
        ],
      ),
    );
  }
}

class WifiLoginForm extends StatefulWidget {
  const WifiLoginForm({super.key});
  @override
  State<WifiLoginForm> createState() => _WifiLoginFormState();
}

class _WifiLoginFormState extends State<WifiLoginForm> {
  final _ip = TextEditingController(text: '192.168.4.1');
  final _user = TextEditingController(text: 'deauther');
  final _pass = TextEditingController(text: 'X4xSWfuK1DtBse');
  bool _busy = false;
  String _err = '';

  void _login() async {
    setState(() => _busy = true);
    try {
      final c = WifiController(_ip.text.trim());
      await c.login(_user.text.trim(), _pass.text.trim());
      if (!mounted) return;
      Navigator.push(context,
          MaterialPageRoute(builder: (_) => ControlPanel(controller: c)));
    } catch (e) {
      setState(() => _err = e.toString());
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const ListTile(
              contentPadding: EdgeInsets.zero,
              leading: Icon(Icons.wifi, color: _rojo),
              title: Text('Conexión WiFi',
                  style: TextStyle(fontWeight: FontWeight.bold)),
              subtitle: Text(
                  'Conéctate a la red "ESP32-Deauther" y luego inicia sesión.'),
            ),
            TextField(
                controller: _ip,
                decoration: const InputDecoration(
                    labelText: 'IP del ESP32',
                    prefixIcon: Icon(Icons.router))),
            TextField(
                controller: _user,
                decoration: const InputDecoration(
                    labelText: 'Usuario', prefixIcon: Icon(Icons.person))),
            TextField(
                controller: _pass,
                decoration: const InputDecoration(
                    labelText: 'Contraseña', prefixIcon: Icon(Icons.lock)),
                obscureText: true),
            const SizedBox(height: 12),
            _busy
                ? const Center(child: CircularProgressIndicator())
                : FilledButton.icon(
                    onPressed: _login,
                    icon: const Icon(Icons.login),
                    label: const Text('Iniciar sesión')),
            if (_err.isNotEmpty)
              Padding(
                padding: const EdgeInsets.only(top: 8),
                child: Text(_err, style: const TextStyle(color: _rojo)),
              ),
          ],
        ),
      ),
    );
  }
}

class BleConnectScreen extends StatefulWidget {
  const BleConnectScreen({super.key});
  @override
  State<BleConnectScreen> createState() => _BleConnectScreenState();
}

class _BleConnectScreenState extends State<BleConnectScreen> {
  List<ScanResult> _results = [];
  bool _scanning = false;
  String _msg = 'Pulsa "Buscar" y elige el dispositivo ESP32-Deauther.';

  Future<bool> _requestBlePermissions() async {
    final statuses = await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.location,
    ].request();
    return statuses[Permission.bluetoothScan]!.isGranted &&
        statuses[Permission.bluetoothConnect]!.isGranted;
  }

  void _scan() async {
    setState(() => _scanning = true);
    _results = [];
    try {
      if (!await _requestBlePermissions()) {
        setState(() => _msg = 'Permisos Bluetooth denegados. Otórgalos en ajustes.');
        return;
      }
      await FlutterBluePlus.startScan(timeout: const Duration(seconds: 5));
      FlutterBluePlus.scanResults.listen((r) {
        setState(() => _results = r);
      });
      await Future.delayed(const Duration(seconds: 5));
      await FlutterBluePlus.stopScan();
    } catch (e) {
      setState(() => _msg = 'Error Bluetooth: $e');
    } finally {
      setState(() => _scanning = false);
    }
  }

  void _connect(ScanResult r) async {
    try {
      final nombre = r.device.platformName.isNotEmpty
          ? r.device.platformName
          : 'dispositivo';
      setState(() => _msg = 'Conectando a $nombre...');
      await r.device.connect();
      final c = BleController(r.device);
      await c.init();

      final pass = await showDialog<String>(
        context: context,
        builder: (ctx) {
          final t = TextEditingController(text: 'X4xSWfuK1DtBse');
          return AlertDialog(
            title: const Text('Acceso Bluetooth'),
            content: TextField(
              controller: t,
              decoration: const InputDecoration(labelText: 'Contraseña'),
              obscureText: true,
            ),
            actions: [
              TextButton(
                  onPressed: () => Navigator.pop(ctx),
                  child: const Text('Cancelar')),
              FilledButton(
                  onPressed: () => Navigator.pop(ctx, t.text),
                  child: const Text('Entrar')),
            ],
          );
        },
      );
      if (pass == null) {
        await c.disconnect();
        return;
      }
      await c.login(pass.trim());
      if (!mounted) return;
      Navigator.push(context,
          MaterialPageRoute(builder: (_) => ControlPanel(controller: c)));
    } catch (e) {
      setState(() => _msg = 'Error: $e');
    }
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const ListTile(
              contentPadding: EdgeInsets.zero,
              leading: Icon(Icons.bluetooth, color: _rojo),
              title: Text('Conexión Bluetooth',
                  style: TextStyle(fontWeight: FontWeight.bold)),
              subtitle: Text('Busca el ESP32 y conéctate sin WiFi.'),
            ),
            FilledButton.icon(
              onPressed: _scanning ? null : _scan,
              icon: const Icon(Icons.search),
              label: Text(_scanning ? 'Buscando...' : 'Buscar dispositivos'),
            ),
            const SizedBox(height: 8),
            Text(_msg),
            const SizedBox(height: 8),
            for (var i = 0; i < _results.length; i++)
              Builder(builder: (_) {
                final r = _results[i];
                final name = r.device.platformName.isNotEmpty
                    ? r.device.platformName
                    : r.advertisementData.localName;
                return Card(
                  child: ListTile(
                    leading: const Icon(Icons.bluetooth, color: _rojo),
                    title: Text(name.isNotEmpty ? name : 'Sin nombre'),
                    subtitle: Text(r.device.remoteId.toString()),
                    trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                    onTap: () => _connect(r),
                  ),
                );
              }),
          ],
        ),
      ),
    );
  }
}

class ControlPanel extends StatefulWidget {
  final DeautherController controller;
  const ControlPanel({super.key, required this.controller});
  @override
  State<ControlPanel> createState() => _ControlPanelState();
}

class _ControlPanelState extends State<ControlPanel> {
  List<Network> _nets = [];
  int? _selected;
  String _reason = reasonCodes.keys.first;
  String _status = '';
  String _cloneStatus = '';
  String _log = '';
  bool _busy = false;
  final _cloneSsid = TextEditingController();
  final _cloneChannel = TextEditingController(text: '6');
  final _clonePass = TextEditingController();

  void _addLog(String s) => setState(() => _log += '$s\n');

  void _scan() async {
    setState(() => _busy = true);
    try {
      final n = await widget.controller.scan();
      setState(() => _nets = n);
      _addLog('Se encontraron ${n.length} redes');
    } catch (e) {
      _addLog('Error al buscar redes: $e');
    } finally {
      setState(() => _busy = false);
    }
  }

  void _desconectarRed() async {
    if (_selected == null) {
      _addLog('Elige primero una red de la lista');
      return;
    }
    setState(() => _busy = true);
    try {
      final r = await widget.controller
          .deauthSingle(_selected!, reasonCodes[_reason]!);
      _addLog('Desconectando red $_selected: $r');
    } catch (e) {
      _addLog('Error: $e');
    } finally {
      setState(() => _busy = false);
    }
  }

  void _desconectarTodas() async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (c) => AlertDialog(
        title: const Text('Desconectar TODAS las redes'),
        content: const Text('El WiFi del ESP32 se apagará. Para detenerlo habrá que '
            'reiniciar la placa. ¿Continuar?'),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(c, false),
              child: const Text('No')),
          FilledButton(
              onPressed: () => Navigator.pop(c, true),
              child: const Text('Sí, continuar')),
        ],
      ),
    );
    if (ok != true) return;
    setState(() => _busy = true);
    try {
      final r = await widget.controller.deauthAll(reasonCodes[_reason]!);
      _addLog('Desconexión global: $r');
    } catch (e) {
      _addLog('Error: $e');
    } finally {
      setState(() => _busy = false);
    }
  }

  void _detener() async {
    setState(() => _busy = true);
    try {
      final r = await widget.controller.stop();
      _addLog('Ataque detenido: $r');
    } catch (e) {
      _addLog('Error: $e');
    } finally {
      setState(() => _busy = false);
    }
  }

  void _refreshStatus() async {
    try {
      final s = await widget.controller.statusRaw();
      setState(() => _status = s);
    } catch (_) {}
    try {
      final c = await widget.controller.cloneStatusRaw();
      setState(() => _cloneStatus = c);
    } catch (_) {}
  }

  void _cloneSelected() async {
    if (_selected == null) {
      _addLog('Elige primero una red para clonar');
      return;
    }
    setState(() => _busy = true);
    try {
      final r = await widget.controller
          .cloneNet(_selected!, _clonePass.text.trim());
      _addLog('Clonando red $_selected: $r');
      await _refreshStatus();
    } catch (e) {
      _addLog('Error del clonador: $e');
    } finally {
      setState(() => _busy = false);
    }
  }

  void _cloneManual() async {
    final ssid = _cloneSsid.text.trim();
    if (ssid.isEmpty) {
      _addLog('Escribe el nombre de la red a clonar');
      return;
    }
    final ch = int.tryParse(_cloneChannel.text.trim()) ?? 1;
    setState(() => _busy = true);
    try {
      final r = await widget.controller
          .cloneCustom(ssid, ch, _clonePass.text.trim());
      _addLog('Clon manual "$ssid": $r');
      await _refreshStatus();
    } catch (e) {
      _addLog('Error del clonador: $e');
    } finally {
      setState(() => _busy = false);
    }
  }

  void _cloneStop() async {
    setState(() => _busy = true);
    try {
      final r = await widget.controller.cloneStop();
      _addLog('Clonador detenido: $r');
      await _refreshStatus();
    } catch (e) {
      _addLog('Error: $e');
    } finally {
      setState(() => _busy = false);
    }
  }

  @override
  void initState() {
    super.initState();
    _refreshStatus();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Panel de control'),
        backgroundColor: _rojo,
        foregroundColor: Colors.white,
      ),
      body: ListView(
        padding: const EdgeInsets.all(12),
        children: [
          // 1 · Redes
          Card(
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  const ListTile(
                    contentPadding: EdgeInsets.zero,
                    leading: Icon(Icons.wifi_find, color: _rojo),
                    title: Text('1 · Redes WiFi cercanas',
                        style: TextStyle(fontWeight: FontWeight.bold)),
                    subtitle: Text('Busca, y toca una red para elegirla.'),
                  ),
                  FilledButton.icon(
                    onPressed: _busy ? null : _scan,
                    icon: const Icon(Icons.search),
                    label: const Text('Buscar redes'),
                  ),
                  const SizedBox(height: 8),
                  if (_nets.isEmpty)
                    const Text('Aún no hay redes. Pulsa «Buscar redes».',
                        style: TextStyle(color: Colors.grey)),
                  for (final n in _nets)
                    RadioListTile<int>(
                      title: Text(n.ssid.isNotEmpty ? n.ssid : '(red oculta)'),
                      subtitle: Text(
                          'Red ${n.num} · Canal ${n.channel} · Señal ${n.rssi} dBm · ${n.enc}'),
                      value: n.num,
                      groupValue: _selected,
                      activeColor: _rojo,
                      onChanged: (v) => setState(() => _selected = v),
                    ),
                ],
              ),
            ),
          ),
          // 2 · Ataques con nombres amigables
          Card(
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  const ListTile(
                    contentPadding: EdgeInsets.zero,
                    leading: Icon(Icons.wifi_off, color: _rojo),
                    title: Text('2 · Desconectar dispositivos',
                        style: TextStyle(fontWeight: FontWeight.bold)),
                    subtitle: Text(
                        'Expulsa temporalmente los dispositivos de una red.'),
                  ),
                  DropdownButtonFormField<String>(
                    value: _reason,
                    items: reasonCodes.keys
                        .map((k) =>
                            DropdownMenuItem(value: k, child: Text(k)))
                        .toList(),
                    onChanged: (v) => setState(() => _reason = v!),
                    decoration: const InputDecoration(
                        labelText: 'Motivo del aviso',
                        prefixIcon: Icon(Icons.info_outline)),
                  ),
                  const SizedBox(height: 8),
                  FilledButton.icon(
                    onPressed: _busy ? null : _desconectarRed,
                    icon: const Icon(Icons.wifi_off),
                    label: const Text('Desconectar la red elegida'),
                  ),
                  const SizedBox(height: 8),
                  FilledButton.icon(
                    onPressed: _busy ? null : _desconectarTodas,
                    icon: const Icon(Icons.warning),
                    style: FilledButton.styleFrom(backgroundColor: _rojo),
                    label: const Text('Desconectar TODAS las redes'),
                  ),
                  const Text(
                      'Ojo: esto apaga el WiFi del ESP32 y hay que reiniciarlo.',
                      style: TextStyle(fontSize: 12, color: Colors.grey)),
                  const SizedBox(height: 8),
                  OutlinedButton.icon(
                    onPressed: _busy ? null : _detener,
                    icon: const Icon(Icons.stop),
                    label: const Text('Detener ataque'),
                  ),
                ],
              ),
            ),
          ),
          // 3 · Clonador
          Card(
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  const ListTile(
                    contentPadding: EdgeInsets.zero,
                    leading: Icon(Icons.copy_all, color: _rojo),
                    title: Text('3 · Clonador de red',
                        style: TextStyle(fontWeight: FontWeight.bold)),
                    subtitle: Text(
                        'Crea una copia de una red para pruebas. Solo redes propias.'),
                  ),
                  Text('Estado del clonador: $_cloneStatus',
                      style: const TextStyle(fontSize: 12)),
                  const SizedBox(height: 6),
                  TextField(
                      controller: _cloneSsid,
                      decoration: const InputDecoration(
                          labelText: 'Nombre de la red a copiar',
                          hintText: 'Ej. MiCasa_WiFi',
                          prefixIcon: Icon(Icons.badge))),
                  Row(
                    children: [
                      Expanded(
                          child: TextField(
                              controller: _cloneChannel,
                              decoration: const InputDecoration(
                                  labelText: 'Canal (1-13)',
                                  prefixIcon: Icon(Icons.tune)),
                              keyboardType: TextInputType.number)),
                      const SizedBox(width: 8),
                      Expanded(
                          child: TextField(
                              controller: _clonePass,
                              decoration: const InputDecoration(
                                  labelText: 'Contraseña (vacío = abierta)',
                                  prefixIcon: Icon(Icons.lock)))),
                    ],
                  ),
                  const SizedBox(height: 8),
                  FilledButton.icon(
                    onPressed: _busy ? null : _cloneSelected,
                    icon: const Icon(Icons.copy),
                    label: const Text('Copiar la red elegida'),
                  ),
                  const SizedBox(height: 8),
                  FilledButton.icon(
                    onPressed: _busy ? null : _cloneManual,
                    icon: const Icon(Icons.edit),
                    label: const Text('Copiar con estos datos'),
                  ),
                  const SizedBox(height: 8),
                  OutlinedButton.icon(
                    onPressed: _busy ? null : _cloneStop,
                    icon: const Icon(Icons.stop),
                    label: const Text('Detener clonador'),
                  ),
                ],
              ),
            ),
          ),
          // 4 · Estado y registro
          Card(
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  const ListTile(
                    contentPadding: EdgeInsets.zero,
                    leading: Icon(Icons.monitor_heart, color: _rojo),
                    title: Text('Estado y registro',
                        style: TextStyle(fontWeight: FontWeight.bold)),
                  ),
                  Text('Estado: $_status',
                      style: const TextStyle(fontSize: 12)),
                  const SizedBox(height: 8),
                  Container(
                    height: 120,
                    padding: const EdgeInsets.all(8),
                    decoration: BoxDecoration(
                      color: Colors.black87,
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: SingleChildScrollView(
                        child: Text(_log,
                            style: const TextStyle(
                                color: Colors.greenAccent, fontSize: 12))),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}
