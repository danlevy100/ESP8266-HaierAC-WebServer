<!DOCTYPE html>
<script src="https://code.highcharts.com/highcharts.js"></script>
<script src="https://code.highcharts.com/modules/exporting.js"></script>
<script src="https://code.highcharts.com/modules/full-screen.js"></script>

<html>
<body>

<div id="container" style="width:100%; height:700px;"></div>
<button id="button">Toggle fullscreen</button>

<label for="start">Start date:</label>

<input type="date" id="start" name="log-start"
       value="2021-08-27"
       min="2021-08-27">

<label for="start">End date:</label>
<input type="date" id="start" name="log-end">

<?php
$db = new SQLite3('AC_DB/ac.db');
#$sql = "SELECT * FROM SensorData";
$sql = "SELECT id, value1, value2, Timestamp FROM SensorData order by Timestamp desc limit 2880";
#$sql = "SELECT id, value1, value2, value3, Timestamp FROM SensorData order by Timestamp desc";
$result = $db->query($sql);

while ($row = $result->fetchArray(SQLITE3_ASSOC)){
    #echo $row['Timestamp'] . ': ' . $row['value1'] . ' °C  ' . $row['value2'] . '%<br/>';
    $sensor_data[] = $row;
  }  

  $readings_time = array_column($sensor_data, 'Timestamp');

  // ******* Uncomment to convert readings time array to your timezone ********
$i = 0;
foreach ($readings_time as $reading){
    //Set timezone to + 3 hours (you can change 4 to any number)
    $readings_time[$i] = date("Y-m-d H:i:s", strtotime("$reading + 3 hours"));
    //$date = new DateTime($readings_time[$i]);
    //$readings_time[$i] = $date->getTimestamp();
    $i += 1;
}

$value1 = json_encode(array_reverse(array_column($sensor_data, 'value1')), JSON_NUMERIC_CHECK);
$value2 = json_encode(array_reverse(array_column($sensor_data, 'value2')), JSON_NUMERIC_CHECK);
$value3 = json_encode(array_reverse(array_column($sensor_data, 'value3')), JSON_NUMERIC_CHECK);
$reading_time = json_encode(array_reverse($readings_time), JSON_NUMERIC_CHECK);

#echo $value1;
#echo $value2;
#echo $value3;
#echo $reading_time;

#$result->free();
unset($db);
?>

<script>

var value1 = <?php echo $value1; ?>;
var value2 = <?php echo $value2; ?>;
var reading_time = <?php echo $reading_time; ?>;

var chartTH = Highcharts.chart('container', {
  chart:{ 
      //renderTo : 'container',
      zoomType: 'xy',
      panning: true,
      panKey: 'shift'
     },
  title: { text: 'Living room temperature and humidity' },
  series: [{
    name: 'Temperature',
    showInLegend: true,
    yAxis: 1,
    data: value1,
    tooltip: {
            valueSuffix: ' °C'
        }    
  }, {
    name: 'Humidity',
    showInLegend: true,
    yAxis: 0,
    data: value2,
    tooltip: {
            valueSuffix: '%'
        } 
  }],
  plotOptions: {
    line: { animation: true,
      dataLabels: { enabled: false }
    }    
  },
  xAxis: { 
    type: 'datetime',
    //tickInterval: 60,
    categories: reading_time,
    labels: {
       format: '{value:%Y-%H-%m}'
    },
  },
  yAxis: [{ // Primary yAxis
        labels: {
            format: '{value} %',
            style: {
                color: Highcharts.getOptions().colors[1]
            }
        },
        title: {
            text: 'Humidity',
            style: {
                color: Highcharts.getOptions().colors[1]
            }
        },
        opposite: true

    }, { // Secondary yAxis
        gridLineWidth: 0,
        title: {
            text: 'Temperature',
            style: {
                color: Highcharts.getOptions().colors[0]
            }
        },
        labels: {
            format: '{value}°C',
            style: {
                color: Highcharts.getOptions().colors[0]
            }
        }
    }],
  credits: { enabled: false },
  navigation: {
        buttonOptions: {
            enabled: false
        }
    }
});

// The button handler
document.getElementById('button').addEventListener('click', function () {
    chartTH.fullscreen.toggle();
    //window.alert('Dan');
});
</script>

</body>
</html>