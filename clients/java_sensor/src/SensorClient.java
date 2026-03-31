import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.HashMap;
import java.util.Map;
import java.util.Random;

public class SensorClient {
    private static final Random RANDOM = new Random();

    private final String host;
    private final int port;
    private final String sensorId;
    private final String sensorType;
    private final int intervalMs;
    private final int timeoutMs;
    private final double anomalyChance;
    private final Integer maxMessages;

    private Socket socket;
    private BufferedReader reader;
    private BufferedWriter writer;

    public SensorClient(
        String host,
        int port,
        String sensorId,
        String sensorType,
        int intervalMs,
        int timeoutMs,
        double anomalyChance,
        Integer maxMessages
    ) {
        this.host = host;
        this.port = port;
        this.sensorId = sensorId;
        this.sensorType = sensorType;
        this.intervalMs = intervalMs;
        this.timeoutMs = timeoutMs;
        this.anomalyChance = anomalyChance;
        this.maxMessages = maxMessages;
    }

    public static void main(String[] args) {
        try {
            Map<String, String> parsedArgs = parseArgs(args);

            String host = requireArg(parsedArgs, "--host");
            int port = Integer.parseInt(requireArg(parsedArgs, "--port"));
            String sensorId = requireArg(parsedArgs, "--id");
            String sensorType = requireArg(parsedArgs, "--type").toUpperCase();

            if (port <= 0 || port > 65535) {
                throw new IllegalArgumentException("Puerto inválido. Debe estar entre 1 y 65535.");
            }

            if (sensorId.trim().isEmpty()) {
                throw new IllegalArgumentException("El sensor_id no puede estar vacío.");
            }

            if (!isValidSensorType(sensorType)) {
                throw new IllegalArgumentException(
                    "Tipo de sensor inválido. Usa TEMP, VIB o POWER."
                );
            }

            int intervalMs = parsedArgs.containsKey("--interval")
                ? (int) (Double.parseDouble(parsedArgs.get("--interval")) * 1000)
                : 3000;

            if (intervalMs <= 0) {
                throw new IllegalArgumentException("--interval debe ser mayor a 0.");
            }

            int timeoutMs = parsedArgs.containsKey("--timeout")
                ? (int) (Double.parseDouble(parsedArgs.get("--timeout")) * 1000)
                : 5000;

            if (timeoutMs <= 0) {
                throw new IllegalArgumentException("--timeout debe ser mayor a 0.");
            }

            double anomalyChance = parsedArgs.containsKey("--anomaly-chance")
                ? Double.parseDouble(parsedArgs.get("--anomaly-chance"))
                : 0.15;

            if (anomalyChance < 0.0 || anomalyChance > 1.0) {
                throw new IllegalArgumentException("--anomaly-chance debe estar entre 0 y 1.");
            }

            Integer maxMessages = parsedArgs.containsKey("--max-messages")
                ? Integer.parseInt(parsedArgs.get("--max-messages"))
                : null;

            if (maxMessages != null && maxMessages < 0) {
                throw new IllegalArgumentException("--max-messages no puede ser negativo.");
            }

            SensorClient client = new SensorClient(
                host,
                port,
                sensorId,
                sensorType,
                intervalMs,
                timeoutMs,
                anomalyChance,
                maxMessages
            );

            client.run();
        } catch (Exception e) {
            System.err.println("[ERROR] " + e.getMessage());
            printUsage();
            System.exit(1);
        }
    }

    public void run() {
        int sentCount = 0;

        try {
            connect();

            String registerResponse = register();
            if (!registerResponse.startsWith("200")) {
                throw new RuntimeException("Registro rechazado: " + registerResponse);
            }

            while (maxMessages == null || sentCount < maxMessages) {
                double value = generateValue();
                String timestamp = utcTimestamp();

                String metricResponse = sendMetric(value, timestamp);
                if (!metricResponse.startsWith("200")) {
                    System.out.println("[WARN] Respuesta inesperada: " + metricResponse);
                }

                sentCount++;
                if (maxMessages != null && sentCount >= maxMessages) {
                    break;
                }

                Thread.sleep(intervalMs);
            }
        } catch (SocketTimeoutException e) {
            System.err.println("[ERROR] Timeout de socket: " + e.getMessage());
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            System.err.println("[ERROR] Hilo interrumpido.");
        } catch (IOException e) {
            System.err.println("[ERROR] Error de red: " + e.getMessage());
        } finally {
            close();
        }
    }

    private void connect() throws IOException {
        socket = new Socket(host, port);
        socket.setSoTimeout(timeoutMs);

        reader = new BufferedReader(
            new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8)
        );
        writer = new BufferedWriter(
            new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8)
        );

        System.out.printf(
            "[INFO] Conectado a %s:%d como %s (%s)%n",
            host, port, sensorId, sensorType
        );
    }

    private void close() {
        try {
            if (socket != null && !socket.isClosed()) {
                try {
                    sendQuit();
                } catch (Exception e) {
                    System.out.println("[WARN] No se pudo enviar QUIT: " + e.getMessage());
                }
            }
        } finally {
            tryClose(writer);
            tryClose(reader);
            tryClose(socket);
            System.out.printf("[INFO] Conexión cerrada para %s%n", sensorId);
        }
    }

    private String register() throws IOException {
        return sendCommand("REGISTER " + sensorId + " " + sensorType);
    }

    private String sendMetric(double value, String timestamp) throws IOException {
        return sendCommand(
            String.format("METRIC %s %.2f %s", sensorId, value, timestamp)
        );
    }

    private String sendPing() throws IOException {
        return sendCommand("PING " + sensorId);
    }

    private String sendStatus() throws IOException {
        return sendCommand("STATUS " + sensorId);
    }

    private String sendQuit() throws IOException {
        return sendCommand("QUIT " + sensorId);
    }

    private String sendCommand(String command) throws IOException {
        writer.write(command);
        writer.write("\n");
        writer.flush();

        System.out.println("[SEND] " + command);

        String response = reader.readLine();
        if (response == null) {
            throw new IOException("El servidor cerró la conexión.");
        }

        System.out.println("[RECV] " + response);
        return response.trim();
    }

    private double generateValue() {
        boolean anomaly = RANDOM.nextDouble() < anomalyChance;

        switch (sensorType) {
            case "TEMP":
                return anomaly ? randomBetween(71.0, 95.0) : randomBetween(20.0, 65.0);
            case "VIB":
                return anomaly ? randomBetween(51.0, 90.0) : randomBetween(5.0, 45.0);
            case "POWER":
                if (anomaly) {
                    return RANDOM.nextBoolean()
                        ? randomBetween(0.0, 9.0)
                        : randomBetween(101.0, 130.0);
                }
                return randomBetween(20.0, 90.0);
            default:
                throw new IllegalStateException("Tipo de sensor no soportado: " + sensorType);
        }
    }

    private static double randomBetween(double min, double max) {
        return min + (max - min) * RANDOM.nextDouble();
    }

    private static String utcTimestamp() {
        // El mock de pruebas acepta ISO-8601 UTC sin fracciones de segundo.
        return Instant.now().truncatedTo(ChronoUnit.SECONDS).toString();
    }

    private static boolean isValidSensorType(String sensorType) {
        return "TEMP".equals(sensorType) ||
               "VIB".equals(sensorType) ||
               "POWER".equals(sensorType);
    }

    private static Map<String, String> parseArgs(String[] args) {
        Map<String, String> parsed = new HashMap<>();

        for (int i = 0; i < args.length; i++) {
            String key = args[i];

            if (!key.startsWith("--")) {
                throw new IllegalArgumentException("Argumento inválido: " + key);
            }

            if (i + 1 >= args.length) {
                throw new IllegalArgumentException("Falta valor para " + key);
            }

            parsed.put(key, args[++i]);
        }

        return parsed;
    }

    private static String requireArg(Map<String, String> args, String key) {
        if (!args.containsKey(key)) {
            throw new IllegalArgumentException("Falta argumento requerido: " + key);
        }
        return args.get(key);
    }

    private static void printUsage() {
        System.out.println();
        System.out.println("Uso:");
        System.out.println("  java SensorClient --host <host> --port <port> --id <sensor_id> --type <TEMP|VIB|POWER> [opciones]");
        System.out.println();
        System.out.println("Opciones:");
        System.out.println("  --interval <segundos>         Intervalo entre métricas. Default: 3.0");
        System.out.println("  --timeout <segundos>          Timeout del socket. Default: 5.0");
        System.out.println("  --anomaly-chance <0..1>       Probabilidad de valor anómalo. Default: 0.15");
        System.out.println("  --max-messages <n>            Cantidad máxima de métricas antes de cerrar.");
        System.out.println();
    }

    private static void tryClose(AutoCloseable closeable) {
        if (closeable == null) {
            return;
        }

        try {
            closeable.close();
        } catch (Exception ignored) {
        }
    }
}