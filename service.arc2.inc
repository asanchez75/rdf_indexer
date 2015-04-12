<?php

/**
 * Search service class using an RDF store server.
 */
class RdfIndexerArc2StoreService extends RdfIndexerBaseService {

  /*
   * Loads ARC2 store this index is connected to.
   */
  public function getStore(SearchApiIndex $index) {
    return arc2_store_get_store($index->server()->options['store_id']);
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
    ctools_include('export');
    if (module_exists('arc2_store')) {
      foreach (ctools_export_crud_load_all('arc2_store_settings') as $store) {
        if (empty($store->disabled)) {
          $options[$store->store_id] = $store->label . ' (' . $store->store_id . ')';
        }
      }
    }

    if (empty($options)) {
      $form['store_id'] = array(
        '#markup' => t('No ARC2 store found. Please install the ARC2 store module and create some stores.'),
      );
    }
    else {
      $form['store_id'] = array(
        '#type' => 'select',
        '#title' => t('ARC2 store'),
        '#description' => t('The ARC2 store where the data should be indexed.'),
        '#options' => $options,
        '#default_value' => $this->options['store_id'],
        '#required' => TRUE,
      );
    }
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
    $output = '';
    $store = arc2_store_settings_load($this->options['store_id']);
    if (!empty($store->settings['endpoint_enabled'])) {
      $url = url($store->settings['endpoint_path'], array('absolute' => TRUE));
      $output .= "<dl>\n  <dt>";
      $output .= t('SPARQL endpoint');
      $output .= "</dt>\n  <dd>";
      $output .= l($url, $url);
      $output .= '</dd>';
      $output .= "\n</dl>";
    }

    return $output;
  }
  
  /*
   * Index an item to the given store
   *
   * @param $item
   *   The rdfx_get_rdf_model() resource
   * @param $store
   *   The ARC2 Store
   * @param $graph
   *   The optional graph URI to update (unused for ARC2)
   */
  public function indexItem($item, $store, $graph = FALSE) {
    $store->insert($item->index, $item->uri);
  }
  
  /*
   * Delete an item from the given store
   *
   * @param $item
   *   The rdfx_get_rdf_model() resource
   * @param $store
   *   The ARC2 Store
   * @param $graph
   *   The optional graph URI to update (unused for ARC2)
   */
  public function deleteItem($item, $store, $graph = FALSE) {
    $store->delete('', $item);
  }
  
  /*
   * Clear the ARC2 store
   *
   * @param $store
   *   The ARC2 Store
   * @param $graph
   *   The optional graph URI to update (unused for ARC2)
   */
  public function clearGraph($store, $graph = FALSE) {
    $store->reset();
  }
  
}