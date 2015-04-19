<?php

/**
 * Search service class using an RDF store server.
 */
class RdfIndexerVirtuosoService extends RdfIndexerBaseService {
  
  /*
   * The external store
   */
  public function getStore(SearchApiIndex $index) {
    return $this->options;
  }
  
  public function getTriplestoreUrl() {
    if ( !isset($this->options['url']) ) {
      watchdog('rdf_indexer', "The URL of the Virtuoso Server has not been specified.", WATCHDOG_WARNING);
    }
    
    return $this->options['url'];
  }
  
  public function getRequestHeaders() {
    $headers = array(
      'Content-Type' => 'application/x-www-form-urlencoded',
    );
    if ( !empty($this->options['authorization']) ) {
      $headers['Authorization'] = $this->options['authorization'];
    }
    return $headers;
  }

  /**
   * Form callback. Might be called on an uninitialized object - in this case,
   * the form is for configuring a newly created server.
   *
   * Returns an empty form by default.
   *
   * @return array
   *   A form array for setting service-specific options.
   */
  public function configurationForm(array $form, array &$form_state) {
    global $base_url;
    
    $settings = isset($this->options) ? $this->options : array();
    $form['url'] = array(
      '#type' => 'textfield',
      '#title' => t('Virtuoso SPARQL Endpoint URL'),
      '#description' => t('The Virtuoso SPARQL Endpoint URL where data will be inserted and deleted via basic authentication. Typically, http://example.com:8890/sparql-auth'),
      '#default_value' => isset($settings['url']) ? $settings['url'] : '',
      '#required' => TRUE,
    );
    $form['graph'] = array(
      '#type' => 'textfield',
      '#title' => t('Graph URI'),
      '#description' => t('The URI of the graph where data will be inserted and deleted.'),
      '#default_value' => isset($settings['graph']) ? $settings['graph'] : $base_url,
      '#required' => TRUE,
    );
    $form['test_query'] = array(
      '#type' => 'textfield',
      '#title' => t('Test SPARQL Query'),
      '#description' => t('A test query that can be used to verify a connection can be established.'),
      '#default_value' => isset($settings['test_query']) ? $settings['test_query'] : 'SELECT ?s WHERE {?s ?p ?o} LIMIT 1',
      '#required' => TRUE,
    );
    $form['query_params'] = array(
      '#type' => 'textfield',
      '#title' => t('Additional Query Parameters'),
      '#description' => t('Specify other query parameters that should be added to SPARQL Update requests.'),
      '#default_value' => isset($settings['query_params']) ? $settings['query_params'] : 'default-graph-uri=&format=text%2Fhtml&timeout=0&debug=on',
    );
    $form['username'] = array(
      '#type' => 'textfield',
      '#title' => t('Username'),
      '#description' => t('The username of the Virtuoso account with SPARQL/Update privileges'),
      '#default_value' => isset($settings['username']) ? $settings['username'] : '',
      '#required' => TRUE,
    );   
    //assume that 'Administer Search API' permission is valid enough to view the password 
    $form['credential'] = array(
      '#type' => 'textfield',
      '#title' => t('Credential'),
      '#description' => t('The credential of the Virtuoso account with SPARQL/Update privileges'),
      '#default_value' => isset($settings['credential']) ? $settings['credential'] : '',
      '#required' => TRUE,
    ); 
    $form['authorization'] = array(
      '#type' => 'hidden',
      '#value' => isset($settings['authorization']) ? $settings['authorization'] : '',
    );    
    return $form;
  }

  /**
   * View this server's settings. Output can be HTML or a render array, a <dl>
   * listing all relevant settings is preferred.
   *
   * The default implementation does a crude output as a definition list, with
   * option names taken from the configuration form.
   */
  public function viewSettings() {
    $settings = isset($this->options) ? $this->options : array();
    if ( !empty($settings) ) {
      require './includes/password.inc';
      $output = "<dl>\n  <dt>";
      $output .= t('Virtuoso SPARQL endpoint');
      $output .= "</dt>\n  <dd>";
      $output .= l($settings['url'], $settings['url']);
      $output .= "</dd>\n  <dt>";
      $output .= t('Graph URI');
      $output .= "</dt>\n<dd>";
      $output .= $settings['graph'];
      $output .= "</dd>\n  <dt>";
      $output .= t('Test Query');
      $output .= "</dt>\n<dd>";
      $output .= $settings['test_query'];
      $output .= "</dd>\n  <dt>";
      $output .= t('Additional Query Parameters');
      $output .= "</dt>\n<dd>";
      $output .= $settings['query_params'];
      $output .= "</dd>\n  <dt>";
      $output .= t('Username');
      $output .= "</dt>\n  <dd>";
      $output .= $settings['username'];
      $output .= "</dd>\n  <dt>";
      $output .= t('Credential');
      $output .= "</dt>\n <dd>";
      $output .= user_hash_password($settings['credential']);
      $output .= "</dd>\n</dl>";
      return $output;
    } 
    else {
      return 'Virtuoso has not been configured.';
    }
  }
  
  /*
   * Index an item to the given store
   *
   * @param $item
   *   The rdfx_get_rdf_model() resource
   * @param $store
   *   The Virtuoso Store options from the SearchApiServer
   * @param $graph
   *   The optional graph URI to update
   * @param $options
   *    Array provided to expansion of the function
   *    Currently, it supports returning the query string
   */
  public function indexItem($item, $store, $graph = FALSE, $options = array()) {
    $graph = $graph ? $graph : $store['graph'];
    $beginning = 'INSERT DATA INTO GRAPH <' . $graph . '> { ';
    $end = ' } ';
    $query = '';
    $queries = array();
    $triple_count = 0;
    foreach($item->index as $subject => $triples){
      //clear out the item in case it already exists 
      $this->deleteItem($subject, $store, $graph, array('no_log' => TRUE));
      
      if( function_exists('drush_log') ){
        drush_log('Re-indexing ' . $subject, 'ok');
      }
      
      foreach($triples as $predicate => $value){
        foreach($value as $triple){
          if (is_string($triple['value'])) {
            $object = '';
            if( isset($triple['type']) && $triple['type'] == 'uri' ){
              $object .= '<'.$triple['value'].'>';
            } else {
              if( !empty($triple['datatype']) ){
                $object .= '"'.$this->prepareString($triple['value']).'"^^'.$triple['datatype'];
              } else if( isset($triple['lang']) && !empty($triple['lang']) ){
                $object .= '"'.$this->prepareString($triple['value']).'"@'.$triple['lang'];
              } else {
                $object .= '"'.$this->prepareString($triple['value']).'"';
              }
            }
            $query .= ' <'.$subject.'> <'.$predicate.'> '.$object.' . ';
            $triple_count += 1;
            //this is to avoid  ERROR:
            //Virtuoso 37000 Error SP031: SPARQL: Internal error: The length of generated SQL text has exceeded 10000 lines of code
            if( $triple_count == 500 ){
              $queries[] = $beginning . $query . $end;
              $triple_count = 0;
              $query = '';
            }
          }
          else {
            watchdog('rdf_indexer', 'See uri = %uri . It can not convert %value into a string.  ', array('%uri' => $item->uri, '%value' => var_export($triple['value'], 1)));
          }

        }
      }
    }
    if( strlen($query) > 0 ){
      $queries[] = $beginning . $query . $end;
    }

    if( isset($options['display']) ){
      $query = '';
      foreach($queries as $q){
        $query .= $q;
      }
      return htmlspecialchars($query);
    } else {
      $result = '';
      foreach($queries as $query){
        $response = $this->executeQuery($query);
        if( $response ){ $result .= $response; }
        else if( function_exists('drush_log') ){
          drush_log($query, 'error');
        }
      }
      return $result;
    }
  }
  
  /**
   * Delete the given URI from the triplestore
   * 
   * @param $item
   *   The rdfx_get_rdf_model() resource
   * @param $store
   *   The Virtuoso Store options from the SearchApiServer
   * @param $graph
   *   The optional graph URI to update
   * @param $options
   *    Array provided to expansion of the function
   *    Currently, it supports returning the query string
   */
  public function deleteItem($item, $store, $graph = FALSE, array $options = array()) {
    $graph = $graph ? $graph : $store['graph'];
    $query = 'WITH <' . $graph . '> DELETE { ?s ?p ?o } WHERE { ?s ?p ?o FILTER (?s = <' . $item . '>)}';
    
    if( !isset($options['no_log']) && function_exists('drush_log') ){
      drush_log('Removing ' . $item, 'ok');
    }
      
    if ( isset($options['display']) ) {
      return htmlspecialchars($query);
    } 
    else {
      return $this->executeQuery($query);
    }
  }
  
  /*
   * Clear the Virtuoso store of all values from this index
   * *** NEEDS UPDATING TO GET ALL ENTITIES FOR THIS INDEX
   *     AND REMOVE ONLY THOSE ***
   *
   * @param $store
   *   The Virtuoso Store options from the SearchApiServer
   * @param $graph
   *   The optional graph URI to update
   * @param $options
   *    Array provided to expansion of the function
   *    Currently, it supports returning the query string
   */
  public function clearGraph($store, $graph = FALSE) {
    $graph = $graph ? $graph : $store['graph'];
    $query = 'CLEAR GRAPH <' . $graph . '>';
    if ( isset($options['display']) ) {
      return htmlspecialchars($query);
    } 
    else {
      return $this->executeQuery($query);
    }
  }
    
  /*
   * Connect to the triplestore and run a query
   *
   * Virtuoso at the /sparql-auth URL implements Digest Authentication
   */
  protected function executeQuery($query) {
    $url = $this->getTriplestoreUrl();
    $config = $this->options;
 
    $data = 'query=' . urlencode($query);
    if( !empty($config['query_params']) ){
      $data .= '&' . $config['query_params'];
    } 
    $content = array(
      'method' => 'POST',
      'headers' => $this->getRequestHeaders(),
      'data' => $data,
    ); 
    $response = drupal_http_request($url, $content);
    if( $response->code == 200 ) {
      return $response->data;
    }
    
    //Unauthorized
    elseif ( $response->code == 401 ) {
    
      $uri = parse_url($this->getTriplestoreUrl());
      $this->authenticate($response->headers['www-authenticate'], $uri['path']);
      //the authenticate method above will have added the new auth headers 
      $content['headers'] = $this->getRequestHeaders(); 
      $response = drupal_http_request($url, $content);
      if ( $response->code == 200 ) {
        watchdog('rdf_indexer', "@data.", array('@data' => $response->data), WATCHDOG_INFO);
        return $response->data;
      } 
      else {
        watchdog('rdf_indexer', "Could not authenticate with the triplestore while trying to run query: @query", array('@query' => $query), WATCHDOG_ERROR);
      }
    } 
    else {
      watchdog('rdf_indexer', "Unknown error[HTTP=@code] occured while trying to run the query: @query", array('@code' => $response->code, '@query' => $query), WATCHDOG_ERROR);
    }
    return FALSE;
  }
  
  /*
   * Build the Authorization header for Virtuoso
   *
   * @param $digest
   *    The digest information passed from Virtuoso via the WWW-Authenticate header
   * @param $uri
   *    The uri path at which the original request was made
   */
  protected function authenticate($data, $uri) {
    $value = explode(' ', $data, 2);
    $type = $value[0];
    
    switch ($type) {
    
      case 'Digest':{
        $digest = array();
        $parts = explode(", ", $value[1]);
        foreach ($parts as $element) {
          $bits = explode("=", $element);
          $digest[$bits[0]] = str_replace('"', '', $bits[1]);
        }
    
        if ( $digest['qop'] == 'auth') {
          $cnonce = time();
          $ncvalue = '00000001';
          $noncebit = $digest['nonce'] . ':' . $ncvalue . ':' . $cnonce . ':auth:' . md5("POST:" . $uri);
          $A = md5($this->server->options['username'] . ':' . $digest['realm'] . ':' . $this->server->options['credential']);
          $respdig = md5("$A:$noncebit");
          $auth_header = 'Digest username="' . $this->server->options['username'] . '", realm="' . $digest['realm'];
          $auth_header .= '", nonce="' . $digest['nonce'] . '", uri="' . $uri . '", algorithm=' . $digest['algorithm'];
          $auth_header .= ', response="' . $respdig . '", opaque="' . $digest['opaque'] . '", qop=' . $digest['qop'];
          $auth_header .= ', nc=' . $ncvalue . ', cnonce="' . $cnonce . '"';
      
          //update the authorization info
          $config = $this->options;
          $config['authorization'] = $auth_header;
          $this->server->options = $config;
          $this->server->save();
      
        } 
        else {
          watchdog('rdf_indexer', "Could not authenticate with the triplestore at URI: @uri because the Digest qop != 'auth'. It was '@qop'", array('@uri' => $uri, '@qop' => $digest['qop'] ), WATCHDOG_ERROR);
        }
        break;
      } //end of 'Digest'
    } //end of switch
  }
  
  /*
   * Clean up literal string values for processing
   */
  protected function prepareString($object = FALSE) {
    if ( !empty($object) ) {
      //strip out control characters
      if ( !ctype_print($object) ) {
        $object = preg_replace('/[\x00-\x1F\x80-\xFF]/', '', $object);
      }
      //escape backslash
      $object = str_replace( "\\", "\\\\", $object );
      //escape double quotes
      $object = str_replace( "\"", "\\\"", $object );
    }
    return $object;
  }

  /*
   * Set a drupal message with the insert/delete queries 
   * or the response data for a given entity type and entity ID
   *
   * @param $entity_type
   *    The type of entity to test
   *
   * @param $id
   *    The entity ID
   *
   * @param $display
   *    A boolean flag that if TRUE just displays the queries. 
   *    If FALSE, the queries are executed and the response data 
   *    is displayed instead
   *
   *  @param $toggle
   *    A string where 'insert' runs just the insert routine,
   *    'delete' runs just the delete routine, and
   *    'both' runs both insert and delete routines
   */
  public function testQueries($entity_type, $id, $display = TRUE, $toggle = 'both') {
    $options = $display ? array('display' => $display) : array();
    $rdf= rdfx_get_rdf_model($entity_type, $id);
    
    $messages = array();
    if ( $toggle == 'both' || $toggle == 'insert') {
      $messages['insert'] = $this->indexItem($rdf, $this->options, FALSE, $options);
    }
    if ( $toggle == 'both' || $toggle == 'delete') {
      $messages['delete'] = $this->deleteItem($rdf->uri, $this->options, FALSE, $options);
    }
    return $messages;
  }
}
