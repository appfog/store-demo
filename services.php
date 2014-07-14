<?php


$services = getenv("VCAP_SERVICES");
$services_json = json_decode($services,true);
?>
<pre>
<?
print_r($services_json);
?>
</pre>
