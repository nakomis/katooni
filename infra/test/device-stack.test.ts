import * as cdk from 'aws-cdk-lib';
import { Template } from 'aws-cdk-lib/assertions';
import { DeviceStack } from '../lib/device-stack';

const synth = (deployEnv: 'sandbox' | 'prod', deviceNames: string[]) => {
  const app = new cdk.App();
  const stack = new DeviceStack(app, 'Test', {
    env: { account: '000000000000', region: 'eu-west-2' },
    deployEnv,
    deviceNames,
  });
  return Template.fromStack(stack);
};

describe('DeviceStack', () => {
  test('one Thing per device, with the attributes Fleet reads', () => {
    const t = synth('sandbox', ['unit-1', 'unit-2']);
    t.resourceCountIs('AWS::IoT::Thing', 2);
    t.hasResourceProperties('AWS::IoT::Thing', {
      ThingName: 'katooni-sandbox-unit-1',
      AttributePayload: {
        Attributes: { project: 'katooni', deviceType: 'cable-tester', capabilities: 'ota,ota-signed' },
      },
    });
  });

  test('the policy pins each device to its own client id and topics', () => {
    const t = synth('prod', ['unit-1']);
    const policies = t.findResources('AWS::IoT::Policy');
    const [policy] = Object.values(policies) as { Properties: { PolicyName: string; PolicyDocument: unknown } }[];
    expect(policy.Properties.PolicyName).toBe('katooni-device-prod');
    const doc = JSON.stringify(policy.Properties.PolicyDocument);
    expect(doc).toContain('client/${iot:Connection.Thing.ThingName}');
    expect(doc).toContain('iot:Connection.Thing.IsAttached');
    expect(doc).toContain('topic/katooni/${iot:Connection.Thing.ThingName}/responses');
    expect(doc).toContain('topicfilter/katooni/${iot:Connection.Thing.ThingName}/commands');
    // Nothing broader than its own topics.
    expect(doc).not.toMatch(/topic\/katooni\/\*|topic\/\*|"iot:\*"/);
  });

  test('no devices: just the policy', () => {
    const t = synth('prod', []);
    t.resourceCountIs('AWS::IoT::Thing', 0);
    t.resourceCountIs('AWS::IoT::Policy', 1);
  });
});
